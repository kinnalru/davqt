
#ifndef PREF_PLAIN_VIEW_H
#define PREF_PLAIN_VIEW_H

#include <QWidget>
#include <QBoxLayout>

#include "base_view.h"

class preferences_widget;

namespace detail{

class connector;

struct plain_view : base_view {
    QWidget* tw;
    preferences_widget* widget;

    plain_view( QWidget* t, detail::connector* )
        : tw(t)
        , widget(0)
    {
        init_ui();
    }

    virtual void insert_widget( preferences_item item )
    {
        if( widget ) delete widget;
        widget = item.widget();
        tw->layout()->addWidget( widget );
    }

    virtual preferences_widget* current_widget() const
    {
        return widget;
    }

protected:
    virtual void init_ui(){
        QBoxLayout* main_box = new QBoxLayout( QBoxLayout::LeftToRight, tw );
        main_box->setContentsMargins( margins() );
    }
};
    

}; //namespace detail

#endif
