/*
 * <one line to give the program's name and a brief idea of what it does.>
 * Copyright (C) 2013  Samoilenko Yuri <kinnalru@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */


#include "tools.h"
#include "settings.h"

#include "main_settings.h"
#include "ui_main_settings.h"

struct main_settings_t::Pimpl
{
    Ui_main_settings ui;
};

main_settings_t::main_settings_t(QWidget* parent)
    : preferences_widget(parent, tr("Main settings"))
    , p_(new Pimpl)
    , lock_change_(false)
{
    set_icon(QIcon("icons:configure.png"));
    set_header(tr("Main settings"));

    
    p_->ui.setupUi(this);
    Q_VERIFY(connect(p_->ui.host, SIGNAL(textChanged(QString)), this, SLOT(int_changed())));
    Q_VERIFY(connect(p_->ui.remotefolder, SIGNAL(textChanged(QString)), this, SLOT(int_changed())));
    Q_VERIFY(connect(p_->ui.localfolder, SIGNAL(textChanged(QString)), this, SLOT(int_changed())));
    Q_VERIFY(connect(p_->ui.username, SIGNAL(textChanged(QString)), this, SLOT(int_changed())));
    Q_VERIFY(connect(p_->ui.password, SIGNAL(textChanged(QString)), this, SLOT(int_changed())));
    Q_VERIFY(connect(p_->ui.interval, SIGNAL(valueChanged(int)), this, SLOT(int_changed())));
}

main_settings_t::~main_settings_t()
{

}

void main_settings_t::update_preferences()
{
    lock_change_ = true;
    settings s;
    
    p_->ui.host->setText(s.host());
    p_->ui.remotefolder->setText(s.remotefolder());
    p_->ui.localfolder->setText(s.localfolder());
    p_->ui.username->setText(s.username());
    p_->ui.password->setText(s.password());
    p_->ui.interval->setValue(s.interval());
    lock_change_ = false;
}


void main_settings_t::accept()
{
    settings s;
    
    s.set_host(p_->ui.host->text());
    s.set_remotefolder(p_->ui.remotefolder->text());
    s.set_localfolder(p_->ui.localfolder->text());
    s.set_username(p_->ui.username->text());
    s.set_password(p_->ui.password->text());
    s.set_interval(p_->ui.interval->value());
    
}

void main_settings_t::reject()
{
    update_preferences();
}

void main_settings_t::reset_defaults()
{
    settings().reset_host();
    settings().reset_remotefolder();
    settings().reset_localfolder();
    settings().reset_username();
    settings().reset_password();
    settings().reset_interval();
    update_preferences();
}

void main_settings_t::int_changed()
{
    if (!lock_change_) emit changed();
}









