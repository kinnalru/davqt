
#ifndef PREF_LIST_VIEW_H
#define PREF_LIST_VIEW_H

#include <QBoxLayout>
#include <QListWidget>
#include <QLabel>
#include <QStackedWidget>

#include "base_view.h"
#include "connector.h"

#include "delegate.h"



namespace detail {

struct list_view : base_view {
    QWidget* tw;

    list_view( QWidget* t, detail::connector* c )
        : tw(t)
    {
        init_ui();

        QObject::connect(
            ui.list,    SIGNAL(currentItemChanged(QListWidgetItem *, QListWidgetItem *)),
            c,          SLOT  (currentItemChanged(QListWidgetItem *, QListWidgetItem *))
        );
        
        ui.list->setItemDelegate( new KLikeListViewDelegate(0) );
    }

    virtual void insert_item(  preferences_item item, preferences_item parent  ) {
        if (parent)
            throw std::runtime_error( "No child allowed in List-represented config UI" );

        preferences_widget* cw = item.widget();
        QListWidgetItem* it = new QListWidgetItem( cw->icon(), cw->name() );
        it->setData( preferences_item::qobject_property_role, qVariantFromValue( item ) );
        item.set_list_item( it );
        ui.list->addItem( it );
        update_width();
    }

    void update_width() {

        int width = 0;
        int rows = ui.list->model()->rowCount();

        for(int i = 0; i < rows; ++i )
            width = std::max( width, ui.list->sizeHintForIndex( ui.list->model()->index(i, 0) ).width() );

        ui.list->setFixedWidth( width + 25 );
    }

    virtual void insert_widget( preferences_item item )
    {
        preferences_widget* cw = item.widget();
        item.set_index( ui.pages->addWidget(cw) );
        ui.pages->setMinimumSize( ui.pages->minimumSizeHint().expandedTo( cw->sizeHint() ) += QSize(5,0) );
    }

    virtual preferences_widget* current_widget() const
    {
        return static_cast<preferences_widget*>( ui.pages->currentWidget() );
    }

    virtual void set_current_widget( preferences_item item )
    {
        preferences_widget* cw = item.widget();
        ui.pages->setCurrentWidget( cw );
        ui.header->setText( cw->header() );
        ui.icon->setPixmap( cw->icon().pixmap( ui.icon->size() ) );
    }

    virtual void set_current_item( preferences_item item )
    {
        ui.list->setCurrentItem( detail::get<QListWidgetItem*>( item.list_item() ) );
    }

protected:
    virtual void init_ui() {
        QBoxLayout* main_box = new QBoxLayout( QBoxLayout::LeftToRight, tw );
        main_box->setContentsMargins( margins() );
        
        QBoxLayout* title_box = new QBoxLayout( QBoxLayout::LeftToRight );
        title_box->setContentsMargins( margins() );
        
        QBoxLayout* pages_box = new QBoxLayout( QBoxLayout::TopToBottom );
        pages_box->setContentsMargins( margins() );
        
        ui.list     = new QListWidget();
        ui.header   = new QLabel();
        ui.icon     = new QLabel();
        ui.pages    = new QStackedWidget();

        ui.icon->setFixedSize( icon_size() );

        main_box->addWidget( ui.list );
        main_box->addLayout( pages_box );

        pages_box->addLayout( title_box );
        pages_box->addWidget( ui.pages );

        title_box->addWidget(ui.header);
        title_box->addWidget(ui.icon);
    }

private:
    struct Ui{
        QListWidget*    list;
        QLabel*         header;
        QLabel*         icon;
        QStackedWidget* pages;
    };

    Ui ui;
};
    
    
} //namespace detail

#endif


