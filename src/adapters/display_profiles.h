// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "json_store.h"
#include <QMap>
#include <QSet>
namespace aitsnap {
QString displayLayoutKey(const std::vector<core::Display>&);
class DisplayProfiles {
public:
    explicit DisplayProfiles(QString path);
    bool ready() const{return diagnostic_.isEmpty();}
    QString diagnostic() const{return diagnostic_;}
    bool enabled() const{return enabled_;}
    bool setEnabled(bool,QString& error);
    bool seed(const Settings&,QString& error);
    bool remember(const core::CaptureProfile&,QString& error);
    bool forget(const std::vector<core::Display>&,QString& error);
    QString boundProfile(const std::vector<core::Display>&) const;
    int count() const{return bindings_.size();}
    // No mutation or I/O. Unknown/deleted bindings never invent a profile.
    bool restoration(const Settings&,const std::vector<core::Display>&,Settings& candidate,QString& error) const;
private:
    QString path_,diagnostic_;
    bool enabled_{true};
    QMap<QString,core::CaptureProfile> bindings_;
    QSet<QString> forgotten_;
    bool persist(QString& error);
};
}
