
#ifndef PREF_BASE_VIEW_H
#define PREF_BASE_VIEW_H

#include <QSize>
#include <QMargins>

namespace detail {

    class base_view {
    public:
        virtual ~base_view(){}

        virtual void insert_item(  preferences_item item, preferences_item parent  ){};
        virtual void insert_widget( preferences_item item ){};
        virtual preferences_widget* current_widget() const{ return 0;};
        virtual void set_current_widget( preferences_item item ){};
        virtual void set_current_item( preferences_item item ){};

        static QSize icon_size(){ return QSize(32, 32); }

        static QMargins margins(){ return QMargins(0,0,0,0); }
        
    protected:
        virtual void init_ui(){};
    };
  
}


#endif
