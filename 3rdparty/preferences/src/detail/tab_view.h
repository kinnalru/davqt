
#ifndef PREF_TAB_VIEW_H
#define PREF_TAB_VIEW_H


#include <QBoxLayout>
#include <QListWidget>
#include <QTabWidget>

#include "base_view.h"
#include "connector.h"

#include "delegate.h"

using namespace std::placeholders;
namespace detail{


struct tab_view : base_view {
    QWidget* tw;

    tab_view( QWidget* t, detail::connector* c )
        : tw(t)
    {
        init_ui();

        QObject::connect(
            ui.list, SIGNAL(currentItemChanged(QListWidgetItem *, QListWidgetItem *)),
            c,       SLOT(currentItemChanged(QListWidgetItem *, QListWidgetItem *))
        );
        
        QObject::connect(
            ui.pages, SIGNAL(currentChanged ( int )),
            c,        SLOT(currentTabChanged ( int ))
        );
        
        ui.list->setItemDelegate( new KLikeListViewDelegate(0) );
    }

    virtual void insert_item(  preferences_item item, preferences_item parent  ) {
        preferences_widget* cw = item.widget();
        if ( parent )
        {
            if ( parent.parent() )
                throw std::runtime_error( "no child allowed in tabs" );
            item.set_list_item( parent.list_item() );
        }
        else
        {
            QListWidgetItem* list_item = new QListWidgetItem( cw->icon(), cw->name() );
            list_item->setData( preferences_item::qobject_property_role, qVariantFromValue( item ) );
            item.set_list_item( list_item );
            ui.list->addItem( list_item );
        }

        update_width();
    }

    void update_width() {

        int width = 0;
        int rows = ui.list->model()->rowCount();

        for(int i = 0; i < rows; ++i )
            width = std::max( width, ui.list->sizeHintForIndex( ui.list->model()->index(i, 0) ).width() );

        ui.list->setFixedWidth( width + 25 );
    }

    void make_tab( QTabWidget* tw, preferences_item item )
    {
        preferences_widget* cw = item.widget();

        item.set_index( tw->addTab( cw, cw->icon(), cw->name() ) );

        tw->setMinimumSize( tw->minimumSizeHint().expandedTo( cw->sizeHint() ) += QSize(5,0) );
    }

    virtual void insert_widget( preferences_item item )
    {
        if ( ui.list->currentItem() == detail::get<QListWidgetItem*>( item.list_item() ) )
            make_tab(ui.pages, item);
    }

    virtual preferences_widget* current_widget() const
    {
        QWidget* w = ui.pages->currentWidget();
        if( !w ) return 0;
        return w->findChild<preferences_widget*>( preferences_widget::object_name );
    }

    void clear()
    {
        for(int i = 0; i<ui.list->count(); ++i )
        {
            QVariant data = ui.list->item(i)->data( preferences_item::qobject_property_role );
            preferences_item item = data.value<preferences_item>();
            item.summon();
        }

        while ( QWidget* w = ui.pages->currentWidget() )
        {
            delete w;
        }

        ui.pages->clear();
    }

    virtual void set_current_widget( preferences_item item )
    {
        if ( item.index() != -1 )
        {
            ui.pages->setCurrentIndex( item.index() );
        }
        else
        {
            clear();
            make_tab(ui.pages, item);
            std::list<preferences_item> childs = item.childs();
            for_each( childs.begin(), childs.end(),
                std::bind(&detail::tab_view::make_tab, this, std::ref(ui.pages), _1) );
        }
    }

    virtual void set_current_item( preferences_item item )
    {
        ui.list->setCurrentItem( detail::get<QListWidgetItem*>( item.list_item() ) );
    }

protected:
    virtual void init_ui(){
        QBoxLayout* main_box = new QBoxLayout( QBoxLayout::LeftToRight, tw );
        main_box->setContentsMargins( margins() );

        QBoxLayout* pages_box = new QBoxLayout( QBoxLayout::TopToBottom );
        pages_box->setContentsMargins( margins() );

        ui.list     = new QListWidget();
        ui.pages    = new QTabWidget();

        main_box->addWidget( ui.list );
        main_box->addLayout( pages_box );

        pages_box->addWidget( ui.pages );
    }

private:
    struct Ui{
        QListWidget*    list;
        QTabWidget*     pages;
    };

    Ui ui;
    
};
    
} //namespace detail

#endif
