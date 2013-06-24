#ifndef CL_PREFERENCES_H
#define CL_PREFERENCES_H

#include <list>
#include <memory>

#include <QDialog>

#include "preferences_item.h"

class QPushButton;

class preferences_widget;
class preferences_item;
class preferences_dialog;

/*! \brief Dialog that manages couple of preferences_widgets
    Provides:
        - accept/reject functionaity
        - reset to defaults
        - subdialogs
*/
class preferences_dialog: public QDialog{
    Q_OBJECT
public:

    enum Type{
        Tree,
        Tab,
        List,
        Plain,
        Auto
    };

    preferences_dialog(Type t, bool native = false, QWidget* parent = 0, Qt::WindowFlags f = 0);
    ~preferences_dialog();

    ///add item at the bottom of other items. At the bottom of childs if  \b parent not 0;
    preferences_item add_item( preferences_widget* cw, const preferences_item& parent = preferences_item() );

public Q_SLOTS:
    ///Tell to dialog that some changes done.
    void changed();
    ///Defalut QDialog accept action
    void accept();
    ///Defalut QDialog reject action
    void reject();

    void block(bool b);
    
    ///Apply changes in current preferences_widget
    void apply_current();
    ///Reject changes in current preferences_widget
    void reject_current();

    void restoreClicked();
    void okClicked();
    void applyClicked();
    void cancelClicked();

    const preferences_item& current_item() const;
    const preferences_widget* current_widget() const;

private Q_SLOTS:

    void button_clicked( int button );
    void item_changed( const preferences_item& new_item );

private:
    Type calculate_type() const;

    void setup_ui();
    void setup_native_ui();

    void setup_clear();

    void reset();
    QPushButton* button(int button) const;

private:
    struct Pimpl;
    std::auto_ptr<Pimpl> p_;
};





#endif

