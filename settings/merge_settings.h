#pragma once

#include <memory>

#include "3rdparty/preferences/src/preferences_widget.h"


class merge_settings_t : public preferences_widget
{
    Q_OBJECT
public:
    explicit merge_settings_t(QWidget* parent = 0);
    virtual ~merge_settings_t();

private Q_SLOTS:
    virtual void update_preferences();
    virtual void accept();
    virtual void reject();
    virtual void reset_defaults();

    void int_changed();

private:
    struct Pimpl;
    std::auto_ptr<Pimpl> p_;
    bool lock_change_;
};

