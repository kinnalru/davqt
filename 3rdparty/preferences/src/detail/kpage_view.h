#ifndef PREF_KPAGE_VIEW_H
#define PREF_KPAGE_VIEW_H

#include <kpagedialog.h>

#include "base_view.h"

namespace detail {

struct kpage_view : base_view {
    QWidget* tw;

    kpage_view( QWidget* t, detail::connector* c )
        : tw(t)
    {
        init_ui();
        
        QObject::connect(
            kpage,  SIGNAL( currentPageChanged ( KPageWidgetItem *, KPageWidgetItem * ) ),
            c,      SLOT  ( currentPageChanged ( KPageWidgetItem *, KPageWidgetItem * ) )
        );

        QObject::connect(
            kpage,  SIGNAL( buttonClicked( KDialog::ButtonCode ) ),
            c,      SLOT  ( buttonClicked( KDialog::ButtonCode ) )
        );
    }

    virtual void insert_item(  preferences_item item, preferences_item parent  ) {
        preferences_widget* cw = item.widget();

        KPageWidgetItem* page_item = new KPageWidgetItem( cw );

        QIcon ic = cw->icon();
        page_item->setIcon( KIcon( ic ) );
        page_item->setName( cw->name() );
        page_item->setProperty( preferences_item::qobject_property, qVariantFromValue( item ) );

        preferences_item::ListItem varitem = detail::make(page_item);

        item.set_list_item( varitem );

        if ( parent )
        {
            KPageWidgetItem* parent_item = detail::get<KPageWidgetItem*>( parent.list_item() );
            kpage->addSubPage( parent_item, page_item );
        }
        else
        {
            kpage->addPage( page_item );
        }

//         kpage->setMinimumSize( kpage->sizeHint() += QSize(0,50) );
    }

    virtual preferences_widget* current_widget() const
    {
        return static_cast<preferences_widget*>( kpage->currentPage()->widget() );
    }

    virtual void set_current_widget( preferences_item item )
    {
        kpage->setCurrentPage( detail::get<KPageWidgetItem*>( item.list_item() ) );
    }

    virtual void set_current_item( preferences_item item )
    {
        kpage->setCurrentPage( detail::get<KPageWidgetItem*>( item.list_item() ) );
    }

    KPageDialog* get_kpage(){ return kpage; }

protected:
    virtual void init_ui() {
        QBoxLayout* main_box = new QBoxLayout( QBoxLayout::LeftToRight, tw );
        main_box->setContentsMargins( margins() );
        main_box->setSizeConstraint( QLayout::SetMinimumSize );
        
        kpage = new KPageDialog;
        kpage->setButtons( KDialog::Ok | KDialog::Cancel | KDialog::Apply | KDialog::Default );
        kpage->setDefaultButton (KDialog::Ok );

        main_box->addWidget( kpage );
    }

private:
    KPageDialog*    kpage;
};

} //namespace detail

#endif

