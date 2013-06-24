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

#ifndef DAVQT_MAIN_SETTINGS_H
#define DAVQT_MAIN_SETTINGS_H

#include <memory>

#include "3rdparty/preferences/src/preferences_widget.h"


class main_settings_t : public preferences_widget
{
    Q_OBJECT
public:
    explicit main_settings_t(QWidget* parent = 0);
    virtual ~main_settings_t();

private Q_SLOTS:
    virtual void update_preferences();
    virtual void accept();
    virtual void reject();
    virtual void reset_defaults();

    void int_changed();

    bool check_url();
    
private:
    struct Pimpl;
    std::auto_ptr<Pimpl> p_;
    bool lock_change_;

};

#endif // MAIN_H
