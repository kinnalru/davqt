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
#include <QFileDialog>

#include "tools.h"
#include "settings.h"

#include "merge_settings.h"
#include "ui_merge_settings.h"

struct merge_settings_t::Pimpl
{
    Ui_merge_settings ui;
};

merge_settings_t::merge_settings_t(QWidget* parent)
    : preferences_widget(parent, tr("Merge"))
    , p_(new Pimpl)
    , lock_change_(false)
{
    set_icon(QIcon("icons:merge.png"));
    set_header(tr("Merge settings"));

    p_->ui.setupUi(this);
    
    p_->ui.compare_btn->setIcon(QIcon::fromTheme("folder-open"));
    p_->ui.merge_btn->setIcon(QIcon::fromTheme("folder-open"));
    
    Q_VERIFY(connect(p_->ui.compare, SIGNAL(textChanged(QString)), this, SLOT(int_changed())));
    Q_VERIFY(connect(p_->ui.merge, SIGNAL(textChanged(QString)), this, SLOT(int_changed())));
    
    Q_VERIFY(::connect(p_->ui.compare_btn, SIGNAL(clicked(bool)), [this] {
      p_->ui.compare->setText(QFileDialog::getOpenFileName(this, tr("Select tool for comparing")));
    }));
    
    Q_VERIFY(::connect(p_->ui.merge_btn, SIGNAL(clicked(bool)), [this] {
      p_->ui.merge->setText(QFileDialog::getOpenFileName(this, tr("Select tool for merging")));
    }));
    
}

merge_settings_t::~merge_settings_t()
{
}

void merge_settings_t::update_preferences()
{
    lock_change_ = true;
    settings s;
    
    p_->ui.compare->setText(s.compare());
    p_->ui.merge->setText(s.merge());
    lock_change_ = false;
}


void merge_settings_t::accept()
{
    settings s;
    
    s.set_compare(p_->ui.compare->text());
    s.set_merge(p_->ui.merge->text());
}

void merge_settings_t::reject()
{
    update_preferences();
}

void merge_settings_t::reset_defaults()
{
    settings().reset_compare();
    settings().reset_merge();
    update_preferences();
}

void merge_settings_t::int_changed()
{
    if (!lock_change_) emit changed();
}








