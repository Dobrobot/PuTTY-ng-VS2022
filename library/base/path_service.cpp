
#include "path_service.h"

#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>

#define _SILENCE_STDEXT_HASH_DEPRECATION_WARNINGS
#include <hash_map>

#include "file_path.h"
#include "file_util.h"
#include "lazy_instance.h"
#include "logging.h"
#include "synchronization/lock.h"

namespace base
{

    bool PathProvider(int key, FilePath* result);
    bool PathProviderWin(int key, FilePath* result);

}

namespace
{

    typedef stdext::hash_map<int, FilePath> PathMap;

    // We keep a linked list of providers.  In a debug build we ensure that no two
    // providers claim overlapping keys.
    struct Provider
    {
        PathService::ProviderFunc func;
        struct Provider* next;
#ifndef NDEBUG
        int key_start;
        int key_end;
#endif
        bool is_static;
    };

    static Provider base_provider =
    {
        base::PathProvider,
        NULL,
#ifndef NDEBUG
        base::PATH_START,
        base::PATH_END,
#endif
        true
    };

    static Provider base_provider_win =
    {
        base::PathProviderWin,
        &base_provider,
#ifndef NDEBUG
        base::PATH_WIN_START,
        base::PATH_WIN_END,
#endif
        true
    };


    struct PathData
    {
        base::Lock lock;
        PathMap cache;        // Cache mappings from path key to path value.
        PathMap overrides;    // Track path overrides.
        Provider* providers;  // Linked list of path service providers.

        PathData()
        {
            providers = &base_provider_win;
        }

        ~PathData()
        {
            Provider* p = providers;
            while(p)
            {
                Provider* next = p->next;
                if(!p->is_static)
                {
                    delete p;
                }
                p = next;
            }
        }
    };

    static base::LazyInstance<PathData> g_path_data(base::LINKER_INITIALIZED);

    static PathData* GetPathData()
    {
        return g_path_data.Pointer();
    }

}

// static
bool PathService::GetFromCache(int key, FilePath* result)
{
    PathData* path_data = GetPathData();
    base::AutoLock scoped_lock(path_data->lock);

    // check for a cached version
    PathMap::const_iterator it = path_data->cache.find(key);
    if(it != path_data->cache.end())
    {
        *result = it->second;
        return true;
    }
    return false;
}

// static
bool PathService::GetFromOverrides(int key, FilePath* result)
{
    PathData* path_data = GetPathData();
    base::AutoLock scoped_lock(path_data->lock);

    // check for an overriden version.
    PathMap::const_iterator it = path_data->overrides.find(key);
    if(it != path_data->overrides.end())
    {
        *result = it->second;
        return true;
    }
    return false;
}

// static
void PathService::AddToCache(int key, const FilePath& path)
{
    PathData* path_data = GetPathData();
    base::AutoLock scoped_lock(path_data->lock);
    // Save the computed path in our cache.
    path_data->cache[key] = path;
}

// TODO(brettw): this function does not handle long paths (filename > MAX_PATH)
// characters). This isn't supported very well by Windows right now, so it is
// moot, but we should keep this in mind for the future.
// static
bool PathService::Get(int key, FilePath* result)
{
    PathData* path_data = GetPathData();
    DCHECK(path_data);
    DCHECK(result);
    DCHECK_GE(key, base::DIR_CURRENT);

    // special case the current directory because it can never be cached
    if(key == base::DIR_CURRENT)
    {
        return base::GetCurrentDirectory(result);
    }

    if(GetFromCache(key, result))
    {
        return true;
    }

    if(GetFromOverrides(key, result))
    {
        return true;
    }

    FilePath path;

    // search providers for the requested path
    // NOTE: it should be safe to iterate here without the lock
    // since RegisterProvider always prepends.
    Provider* provider = path_data->providers;
    while(provider)
    {
        if(provider->func(key, &path))
        {
            break;
        }
        DCHECK(path.empty()) << "provider should not have modified path";
        provider = provider->next;
    }

    if(path.empty())
    {
        return false;
    }

    AddToCache(key, path);

    *result = path;
    return true;
}

bool PathService::Override(int key, const FilePath& path)
{
    PathData* path_data = GetPathData();
    DCHECK(path_data);
    DCHECK_GT(key, base::DIR_CURRENT) << "invalid path key";

    FilePath file_path = path;

    // Make sure the directory exists. We need to do this before we translate
    // this to the absolute path because on POSIX, AbsolutePath fails if called
    // on a non-existant path.
    if(!base::PathExists(file_path) && !base::CreateDirectory(file_path))
    {
        return false;
    }

    // We need to have an absolute path, as extensions and plugins don't like
    // relative paths, and will glady crash the browser in CHECK()s if they get a
    // relative path.
    if(!base::AbsolutePath(&file_path))
    {
        return false;
    }

    base::AutoLock scoped_lock(path_data->lock);

    // Clear the cache now. Some of its entries could have depended
    // on the value we are overriding, and are now out of sync with reality.
    path_data->cache.clear();

    path_data->cache[key] = file_path;
    path_data->overrides[key] = file_path;

    return true;
}

void PathService::RegisterProvider(ProviderFunc func, int key_start,
                                   int key_end)
{
    PathData* path_data = GetPathData();
    DCHECK(path_data);
    DCHECK_GT(key_end, key_start);

    base::AutoLock scoped_lock(path_data->lock);

    Provider* p;

#ifndef NDEBUG
    p = path_data->providers;
    while(p)
    {
        DCHECK(key_start >= p->key_end || key_end <= p->key_start) <<
            "path provider collision";
        p = p->next;
    }
#endif

    p = new Provider;
    p->is_static = false;
    p->func = func;
    p->next = path_data->providers;
#ifndef NDEBUG
    p->key_start = key_start;
    p->key_end = key_end;
#endif
    path_data->providers = p;
}