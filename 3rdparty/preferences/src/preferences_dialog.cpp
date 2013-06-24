#include <iostream>
#include <stdexcept>
#include <functional>

#include <QPointer>
#include <QPushButton>
#include <QMessageBox>

#include "ui_preferences_dialog.h"

#include "detail/views.h"
#include "detail/connector.h"

#include "preferences_widget.h"
#include "preferences_dialog.h"

using namespace std::placeholders;

#ifndef USE_KDE_DIALOG
    const int kde_native = 0;
#else
    #include <kpagedialog.h>
    #include <kpushbutton.h>
    const int kde_native = 1;
#endif

struct plain_tag;
struct tree_tag;
struct tab_tag;
struct list_tag;
struct kde_native_tag;

template <typename Tag>
struct preferences_dialog_traits{};


template <>
struct preferences_dialog_traits<plain_tag>
{
    static const preferences_dialog::Type type = preferences_dialog::Plain;
    typedef detail::plain_view view;
};

template <>
struct preferences_dialog_traits<tree_tag>
{
    static const preferences_dialog::Type type = preferences_dialog::Tree;
    typedef detail::tree_view view;
};

template <>
struct preferences_dialog_traits<tab_tag>
{
    static const preferences_dialog::Type type = preferences_dialog::Tab;
    typedef detail::tab_view view;
};

template <>
struct preferences_dialog_traits<list_tag>
{
   static const preferences_dialog::Type type = preferences_dialog::List;
    typedef detail::list_view view;
};

#ifdef USE_KDE_DIALOG

template <>
struct preferences_dialog_traits<kde_native_tag>
{
    static const preferences_dialog::Type type = preferences_dialog::Auto;
    typedef detail::kpage_view view;
};

KPageDialog::FaceType kpage_type( preferences_dialog::Type type )
{
    switch( type )
    {
        case preferences_dialog::Tree:  return KPageDialog::Tree;
        case preferences_dialog::Tab:   return KPageDialog::Tabbed;
        case preferences_dialog::List:  return KPageDialog::List;
        case preferences_dialog::Plain: return KPageDialog::Plain;
        default: return KPageDialog::Auto;
    }
}

KDialog::ButtonCode kbutton( QDialogButtonBox::StandardButton button )
{
    switch (button)
    {
        case QDialogButtonBox::Apply:   return KDialog::Apply;
        case QDialogButtonBox::Cancel:  return KDialog::Cancel;
        case QDialogButtonBox::Ok:      return KDialog::Ok;
        case QDialogButtonBox::RestoreDefaults:      return KDialog::Default;
        default: return KDialog::None;
    }
}

#endif


/// struct that creates and holds concrete helpers as generic functors
struct preferences_dialog::Pimpl { 

    Pimpl():template_widget(0), block(false) {}

    template <typename Tag>
    void init_ui()
    {
        bv.reset( new typename preferences_dialog_traits<Tag>::view( template_widget.get(), connector) );
        type = preferences_dialog_traits<Tag>::type;
    }

    void add_item( preferences_item item, preferences_item parent )
    { bv->insert_item(item, parent); }

    void add_widget( preferences_item item )
    { bv->insert_widget(item); }

    void set_current_widget( preferences_item item )
    { bv->set_current_widget(item); }

    preferences_widget* current_widget() const
    { return bv->current_widget(); }

    void set_current_item(preferences_item item)
    { bv->set_current_item(item); }



    std::auto_ptr<detail::base_view> bv;

    //main dialog ui object
    Ui_preferences_dialog_form ui;

    std::list<preferences_item> items;

    preferences_item    current_item;

    Type initial_type;
    Type type;

    std::auto_ptr<QWidget> template_widget;

#ifdef USE_KDE_DIALOG
    KPageDialog* page_dialog;
#endif

    detail::connector* connector;
    bool native;
    bool block;
};




preferences_dialog::preferences_dialog(Type t, bool native, QWidget * parent, Qt::WindowFlags f)
    : QDialog( parent, f )
    , p_( new Pimpl )
{
    p_->initial_type = t;
    p_->native = native && kde_native;

    if ( kde_native && p_->native  )
        setup_native_ui();
    else
        setup_ui();

    button( QDialogButtonBox::RestoreDefaults )->setAutoDefault(false);
    button( QDialogButtonBox::Ok )->setDefault(true);

    connect(p_->connector,SIGNAL( item_changed( preferences_item ) ),
            SLOT( item_changed( preferences_item ) ) );

    connect(p_->connector,SIGNAL( button_clicked( int ) ),
            SLOT( button_clicked( int ) ) );

    reset();
}

preferences_dialog::~ preferences_dialog()
{}

void preferences_dialog::setup_ui()
{
    p_->ui.setupUi(this);
    p_->connector = new detail::connector( this, p_->items, p_->ui.buttons);

    resize(QSize(10,10));

    setup_clear();
    switch (p_->initial_type)
    {
        case Tree:  p_->init_ui<tree_tag>();  break;
        case Tab:   p_->init_ui<tab_tag>();   break;
        case List:  p_->init_ui<list_tag>();  break;
        case Auto:  p_->init_ui<plain_tag>(); break;
        case Plain: p_->init_ui<plain_tag>(); break;
    }

    connect(p_->ui.buttons, SIGNAL( clicked( QAbstractButton* ) ),
        p_->connector, SLOT( buttonClicked( QAbstractButton* ) ) );
}

void preferences_dialog::setup_native_ui()
{
#ifdef USE_KDE_DIALOG
    p_->connector = new detail::connector( this, p_->items, 0);
    p_->template_widget.reset( new QWidget );

    ( new QVBoxLayout(this) )->setContentsMargins(0,0,0,0);
    layout()->setSizeConstraint( QLayout::SetMinimumSize );
    layout()->addWidget( p_->template_widget.get() );

    p_->init_ui<kde_native_tag>();

    p_->page_dialog = static_cast<detail::kpage_view*>( p_->bv.get() )->get_kpage();
    p_->page_dialog->setFaceType( kpage_type( p_->initial_type ) );

#endif
}

void preferences_dialog::setup_clear()
{
    for_each( p_->items.begin(), p_->items.end(), std::bind(&preferences_item::summon, _1) );

    p_->template_widget.reset( new QWidget );
    p_->ui.main_widget->layout()->addWidget( p_->template_widget.get() );
}

preferences_item preferences_dialog::add_item(preferences_widget * cw, const preferences_item& parent )
{
    if ( p_->initial_type == Plain && p_->items.size() )
        throw std::runtime_error("plain type occured!");

    preferences_item item( cw, parent );
    p_->items.push_back(item);

    if ( p_->items.size() == 1)
        p_->current_item = item;

//     cw->setFixedSize( cw->sizeHint()+=QSize(10,800) );
//     cw->setFixedSize( QSize(500,500) );

    connect( cw, SIGNAL( changed() ), SLOT( changed() ) );
    connect( cw, SIGNAL( block(bool)) , SLOT( block(bool) ) );
    if ( !p_->native )
    {
        if ( p_->initial_type == Auto )
        {
            Type t = calculate_type();
            if ( t != p_->type )
            {
                p_->type = t;
                setup_clear();
                switch (p_->type)
                {
                    case Plain: p_->init_ui<plain_tag>(); break;
                    case Tree:  p_->init_ui<tree_tag>();  break;
                    case Tab:   p_->init_ui<tab_tag>();   break;
                    case List:  p_->init_ui<list_tag>();  break;
                    default:;
                }
                std::list<preferences_item>::iterator it = p_->items.begin();
                for(; it != p_->items.end(); ++it)
                {
                    p_->add_item( *it, it->parent() );
                    p_->add_widget( *it );
                }
                if( p_->current_item )
                {
                    p_->set_current_item( p_->current_item );
                    p_->current_item.widget()->update_preferences();
                    p_->set_current_widget( p_->current_item );
                }
                return item;
            }
        }
    }

    p_->add_item( item, parent );
    p_->add_widget( item );

    if ( p_->items.size() == 1 )
    {
        p_->current_item = item;
        p_->set_current_item( p_->current_item );
        p_->current_item.widget()->update_preferences();
        p_->set_current_widget( p_->current_item );
    }
    return item;
}

void preferences_dialog::changed()
{
    button( QDialogButtonBox::RestoreDefaults )->setEnabled(true);
    button( QDialogButtonBox::Apply )->setEnabled(true && !p_->block);
}

void preferences_dialog::accept()
{ okClicked(); }

void preferences_dialog::reject()
{ cancelClicked(); }

void preferences_dialog::block(bool b)
{
    p_->block = b;
    button(QDialogButtonBox::Ok)->setEnabled(!b);
    button(QDialogButtonBox::Apply)->setEnabled(!b); 
}

void preferences_dialog::apply_current()
{
    p_->current_item.widget()->accept();
    button( QDialogButtonBox::Apply )->setEnabled(false);
}

void preferences_dialog::reject_current()
{
    p_->current_item.widget()->reject();
    button( QDialogButtonBox::Apply )->setEnabled(false);
}

void preferences_dialog::restoreClicked()
{
    QMessageBox::StandardButton ret = QMessageBox::question ( this,
        tr("Restore to defaults"),
        tr("Continue to restore defaults settings?" ),
        QMessageBox::Ok |  QMessageBox::Cancel );

    if ( ret == QMessageBox::Cancel ) return;

    p_->block = false;
    if ( button(QDialogButtonBox::Apply)->isEnabled() ) reject_current();
    p_->current_item.widget()->reset_defaults();
    button( QDialogButtonBox::RestoreDefaults )->setEnabled(false);
    button( QDialogButtonBox::Apply )->setEnabled(false);
}

void preferences_dialog::applyClicked()
{ apply_current(); }


void preferences_dialog::okClicked()
{
    if ( button(QDialogButtonBox::Apply)->isEnabled() ) apply_current();
    QDialog::accept();
}

void preferences_dialog::cancelClicked()
{
    if ( button(QDialogButtonBox::Apply)->isEnabled() ) reject_current();
    QDialog::reject();
}

const preferences_item& preferences_dialog::current_item() const
{
    return p_->current_item;
}

const preferences_widget* preferences_dialog::current_widget() const
{
    return p_->current_widget();
}

void preferences_dialog::button_clicked(int button)
{
    switch ( button )
    {
        case QDialogButtonBox::RestoreDefaults: restoreClicked(); break;
        case QDialogButtonBox::Ok:              okClicked(); break;
        case QDialogButtonBox::Apply:           applyClicked(); break;
        case QDialogButtonBox::Cancel:          cancelClicked(); break;
        default:
            std::cerr<<"unknown button"<<std::endl;
    }
}

void preferences_dialog::item_changed(const preferences_item& new_item)
{
    if( new_item == p_->current_item ) return;

    new_item.widget()->update_preferences();
    
    if( button(QDialogButtonBox::Apply)->isEnabled() )
    {
        QMessageBox::StandardButton ret = QMessageBox::question ( this,
            tr("Apply settings"),
            tr("Current page has unsaved changes.\nApply this changes?" ),
            QMessageBox::Ok | QMessageBox::Reset | QMessageBox::Cancel );

        if ( ret == QMessageBox::Ok )
        {
            apply_current();
        }
        else if( ret == QMessageBox::Reset )
        {
            reject_current();
        }
        else if( ret == QMessageBox::Cancel )
        {
            p_->set_current_widget( p_->current_item );
            return;
        }
    }

    p_->current_item = new_item;
    p_->set_current_widget( p_->current_item );
    p_->set_current_item( p_->current_item );
    return;
}

preferences_dialog::Type preferences_dialog::calculate_type() const
{
    if ( p_->items.size() == 1 )
        return Plain;
    std::list<preferences_item>::iterator it = p_->items.begin();
    Type ret = List;
    for (; it != p_->items.end(); ++it)
    {
        if ( it->parent() )
        {
            ret = Tab;
            if ( it->parent().parent() )
            {
                ret = Tree;
                break;
            }
        }
    }
    return ret;
}


void preferences_dialog::reset()
{
    button( QDialogButtonBox::RestoreDefaults )->setEnabled(true);
    button( QDialogButtonBox::Ok )->setEnabled(true);
    button( QDialogButtonBox::Cancel )->setEnabled(true);
    button( QDialogButtonBox::Apply )->setEnabled(false);
}

QPushButton * preferences_dialog::button(int button) const
{
    QPushButton* ret(0);

#ifdef USE_KDE_DIALOG
    if ( kde_native && p_->native )
    {
        ret =  p_->page_dialog->button( kbutton( QDialogButtonBox::StandardButton(button) ) );
    }
    else
#endif
    ret =  p_->ui.buttons->button( QDialogButtonBox::StandardButton(button) );

    if ( !ret )
        throw std::runtime_error("no such button");
    return ret;
}









