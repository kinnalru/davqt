

#ifndef PREF_TREE_VIEW_H
#define PREF_TREE_VIEW_H

#include <QBoxLayout>
#include <QTreeWidget>
#include <QLabel>
#include <QStackedWidget>

#include "base_view.h"
#include "connector.h"

namespace detail{


struct tree_view : base_view {
    QWidget* tw;

    tree_view( QWidget* t, detail::connector* c )
        : tw(t)
    {
        init_ui();

        QObject::connect(
            ui.list, SIGNAL(currentItemChanged(QTreeWidgetItem *, QTreeWidgetItem *)),
            c,       SLOT(currentItemChanged(QTreeWidgetItem *, QTreeWidgetItem *))
        );
    }

    virtual void insert_item(  preferences_item item, preferences_item parent  ) {
        preferences_widget* cw = item.widget();

        QTreeWidgetItem* list_item = new QTreeWidgetItem();
        list_item->setIcon(0, cw->icon() );
        list_item->setText(0, cw->name() );
        list_item->setData(0, preferences_item::qobject_property_role, qVariantFromValue( item ) );

        item.set_list_item( list_item );

        if (parent)
        {
            QTreeWidgetItem* parent_list_item = detail::get<QTreeWidgetItem*>(parent.list_item());
            parent_list_item->addChild( list_item );
        }
        else
        {
            ui.list->addTopLevelItem(list_item);
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
        ui.list->setCurrentItem( detail::get<QTreeWidgetItem*>( item.list_item() ) );
    }

protected:
    virtual void init_ui() {
        QBoxLayout* main_box = new QBoxLayout( QBoxLayout::LeftToRight, tw );
        main_box->setContentsMargins( margins() );

        QBoxLayout* title_box = new QBoxLayout( QBoxLayout::LeftToRight );
        title_box->setContentsMargins( margins() );

        QBoxLayout* pages_box = new QBoxLayout( QBoxLayout::TopToBottom );
        pages_box->setContentsMargins( margins() );

        ui.list     = new QTreeWidget();
        ui.header   = new QLabel();
        ui.icon     = new QLabel();
        ui.pages    = new QStackedWidget();

        ui.list->setHeaderHidden(true);

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
        QTreeWidget*    list;
        QLabel*         header;
        QLabel*         icon;
        QStackedWidget* pages;
    };

    Ui ui;
};


} //namespace detail

#endif
