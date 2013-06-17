#include <stdexcept>

#include <functional>

#include <QPointer>

#include "preferences_widget.h"
#include "preferences_item.h"

using namespace std::placeholders;
const char* preferences_item::qobject_property = "c_item_property";
const int   preferences_item::qobject_property_role = Qt::UserRole + 11;

struct preferences_item::Pimpl{

    Pimpl() :index(-1) {}

    ~Pimpl() { if( cw ) delete cw.data(); }

    QPointer<preferences_widget> cw;

    //refference to item in list/tree
    preferences_item::ListItem list_item;
    //QTabWidget index
    int index;

    PreferencesItemList childs;
    preferences_item parent;
};

preferences_item::preferences_item()
{}

preferences_item::preferences_item( preferences_widget* cw, preferences_item parent)
    : p_( new Pimpl )
{
    p_->cw = cw;
    if ( parent ) parent.add_child(*this);
    p_->parent = parent;
}
preferences_item::~ preferences_item()
{}

bool preferences_item::operator ==(const preferences_item & other) const
{ return (p_ == other.p_); }

bool preferences_item::operator !=(const preferences_item & other) const
{ return (p_ != other.p_); }


preferences_item::operator bool() const
{ return p_.get(); }

preferences_widget * preferences_item::widget() const
{
    if (!p_) throw std::runtime_error("preferences_item::widget");
    return p_->cw;
}

void preferences_item::summon()
{
    if (!p_) throw std::runtime_error("preferences_item::summon");
    if( p_->cw ) p_->cw->setParent(0);
    p_->index = -1;
    for_each( p_->childs.begin(), p_->childs.end(), bind( &preferences_item::summon, _1 ) );
}

void preferences_item::set_index(int index)
{
    if (!p_) throw std::runtime_error("preferences_item::set_index");
    p_->index = index;
}

int preferences_item::index() const
{
    if (!p_) throw std::runtime_error("preferences_item::index");
    return p_->index;
}

void preferences_item::set_list_item(const ListItem& list_item)
{
    if (!p_) throw std::runtime_error("preferences_item::set_list_item");
    p_->list_item = list_item;
}

preferences_item::ListItem preferences_item::list_item() const
{
    if (!p_) throw std::runtime_error("preferences_item::list_item");
    return p_->list_item;
}

void preferences_item::add_child(preferences_item child)
{
    if (!p_) throw std::runtime_error("preferences_item::add_child");
    p_->childs.push_back(child);
}

PreferencesItemList preferences_item::childs() const
{
    if (!p_) throw std::runtime_error("preferences_item::childs");
    return p_->childs;
}

preferences_item preferences_item::parent() const
{
    if (!p_) throw std::runtime_error("preferences_item::parent");
    return p_->parent;
}


