#include <QIcon>
#include <QPushButton>
#include <QVBoxLayout>
#include <QMessageBox>

#include "preferences_widget.h"

const QString preferences_widget::object_name = "preferences_widget";

struct preferences_widget::Pimpl{

    Pimpl(){}

    QString name;
    QString header;
    QIcon icon;
};

preferences_widget::preferences_widget( QWidget * parent, const QString& name, Qt::WindowFlags f )
    : QWidget(parent, f)
    , p_( new Pimpl )
{
    setObjectName( object_name );
    set_name( name );
//     QVBoxLayout* lay = new QVBoxLayout(this);
//     QPushButton* pb = new QPushButton(name,this);
//     connect( pb, SIGNAL( clicked() ), SIGNAL( changed() ) );
//     lay->addWidget(pb);
}

preferences_widget::~ preferences_widget()
{
//     QMessageBox::information(0, "preferences_widget", "Deleted!" + name());
}

void preferences_widget::set_header(const QString & header)
{ p_->header = header; }

const QString & preferences_widget::header() const
{ return p_->header; }

void preferences_widget::set_name(const QString & name)
{ p_->name = name; }


const QString & preferences_widget::name() const
{ return p_->name; }

void preferences_widget::set_icon(const QIcon & icon)
{ p_->icon = icon; }

const QIcon & preferences_widget::icon() const
{ return p_->icon; }

void preferences_widget::update_preferences()
{
//     QMessageBox::information(0, "preferences_widget", "updated" + name());
}


void preferences_widget::accept()
{
//     QMessageBox::information(0, "preferences_widget", "accepted" + name());
}

void preferences_widget::reject()
{
//     QMessageBox::information(0, "preferences_widget", "rejected" + name());
}

void preferences_widget::reset_defaults()
{
//     QMessageBox::information(0, "preferences_widget", "RESET" + name());
}

void preferences_widget::setEnabled(bool b)
{
    emit enabled(b);
    QWidget::setEnabled(b);
}






















