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

#include <QDebug>

#include "tools.h"
#include "settings.h"

#include "sync_settings.h"
#include "ui_sync_settings.h"

struct sync_settings_t::Pimpl
{
    Ui_sync_settings ui;
};

sync_settings_t::sync_settings_t(QWidget* parent)
    : preferences_widget(parent, tr("Sync"))
    , p_(new Pimpl)
    , lock_change_(false)
{
    set_icon(QIcon("icons:sync.png"));
    set_header(tr("Sync settings"));

    
    p_->ui.setupUi(this);
    Q_VERIFY(connect(p_->ui.interval, SIGNAL(valueChanged(int)), this, SLOT(int_changed())));
    Q_VERIFY(connect(p_->ui.autosync, SIGNAL(clicked(bool)), this, SLOT(int_changed())));
}

sync_settings_t::~sync_settings_t()
{

}

void sync_settings_t::update_preferences()
{
    lock_change_ = true;
    settings s;
    
    p_->ui.interval->setValue(s.interval());
    p_->ui.autosync->setChecked(s.enabled());
    lock_change_ = false;
}


void sync_settings_t::accept()
{
    settings s;
    
    s.set_interval(p_->ui.interval->value());
    s.set_enabled(p_->ui.autosync->isChecked());
}

void sync_settings_t::reject()
{
    update_preferences();
}

void sync_settings_t::reset_defaults()
{
    settings().reset_interval();
    update_preferences();
}

void sync_settings_t::int_changed()
{
    if (!lock_change_) emit changed();
}








