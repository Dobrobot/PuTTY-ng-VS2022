#ifndef PUTTY_VIEW_H
#define PUTTY_VIEW_H

#include "view/view.h"

namespace view
{

    // ʵ���޴��ڵ�richedit�ؼ�.
    class PuttyView : public View
	{
	public:
		static const char kViewClassName[];

        explicit PuttyView();
        virtual ~PuttyView();

	protected:
        virtual void Layout();

	};

}

#endif