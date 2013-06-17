#ifndef CL_PREFERENCES_ITEM_H
#define CL_PREFERENCES_ITEM_H

#include <memory>
#include <tuple>

#include <QVariant>

class QTreeWidgetItem;
class QListWidgetItem;

#ifdef USE_KDE_DIALOG
class KPageWidgetItem;
#endif

namespace detail {
	
enum accessor{
	tree_item,
	list_item,
	kpage_item
};

template <typename T>
struct item_traits {};

template <>
struct item_traits<QTreeWidgetItem*> { enum { value = 0}; };

template <>
struct item_traits<QListWidgetItem*> { enum { value = 1}; };

#ifdef USE_KDE_DIALOG
template <>
struct item_traits<KPageWidgetItem*> { enum { value = 2}; };
#endif

typedef std::tuple<
    QTreeWidgetItem*,
    QListWidgetItem*
#ifdef USE_KDE_DIALOG
    ,KPageWidgetItem*
#endif
>  ListItem;


template <typename ItemType>
ItemType get(ListItem item) {return std::get<item_traits<ItemType>::value>(item);}

template <typename ItemType>
ListItem make(ItemType i) {ListItem item; std::get<item_traits<ItemType>::value>(item) = i; return item;}

} //namespace detail

class preferences_widget;
class preferences_item;

typedef std::list<preferences_item> PreferencesItemList;

///Class thar represents an entry in preferences_dialog
class preferences_item{
public:

    static const char* qobject_property;
    static const int qobject_property_role;

    ///Type for storing id of item in list/tree, where current item inserted
    typedef detail::ListItem ListItem;

    ///En empty constructor
    preferences_item();

    ///Creates an item that owns preferences_widget and attach it to preferences_dialog
    preferences_item( preferences_widget* cw, preferences_item parent);

    ~preferences_item();

    bool operator==(const preferences_item& other) const;
    bool operator!=(const preferences_item& other) const;

    ///Is item Not empty?
    operator bool() const;

    ///Add child to the item hierarhy
    void add_child(preferences_item);

    ///Get parent of the item
    preferences_item parent() const;

    ///Get all childs of the item
    PreferencesItemList childs() const;

    ///Get preferences_widget owned by this item
    preferences_widget* widget() const;

//private:

    ///Return owning of the widget( setParent(0) and other... )
    void summon();
    
    void set_index(int index);
    int index() const;

    
	template <typename ItemType>
	void set_list_item(ItemType item) {set_list_item(detail::make(item));}
	
    void set_list_item( const ListItem& );
    ListItem list_item() const;

private:
    struct Pimpl;
    std::shared_ptr<Pimpl> p_;
};

Q_DECLARE_METATYPE(preferences_item);

#endif

