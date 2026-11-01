// SPDX-License-Identifier: Apache-2.0
#include "main_window.h"
#include "desktop/appearance.h"
#include "desktop/workspace_views.h"
#include "adapters/settings_commands.h"
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileInfo>
#include <QInputDialog>
#include <QLineEdit>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QScopedValueRollback>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QUuid>
#include <algorithm>
namespace aitsnap {
static QString qs(const std::string& s) { return QString::fromUtf8(s.c_str()); }
static std::string freshId() { return QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString(); }
static QStringList selectedIds(QListWidget* list)
{
    QStringList ids;
    for (const auto* item : list->selectedItems()) if (!item->data(Qt::UserRole).toString().isEmpty()) ids.append(item->data(Qt::UserRole).toString());
    return ids;
}
bool MainWindow::commitObjects(const Settings& candidate, const QJsonArray& rules)
{
    QString error; const bool changedRules = rules != eventRules_;
    if (!persistSettingsCommand(configurationPath_, candidate, changedRules ? &rules : nullptr, error)) {
        if (QFile::exists(configurationPath_ + ".before-management.json")) configurationBlocked_ = true;
        setStatus(uiText("配置操作未完成：") + error, true); return false;
    }
    settings_ = candidate; eventRules_ = rules; overviewOutputKey_.clear();
    sourceCards_->setProperty("selectionInitialized",false); overviewSources_->setProperty("selectionInitialized",false);
    { const QSignalBlocker block(profiles_); profiles_->clear(); for (const auto& p : settings_.profiles) profiles_->addItem(qs(p.name)); profiles_->setCurrentIndex(settings_.activeProfile); }
    rebuildProfileMenu(); loadProfile(); setStatus(uiText("配置已保存，历史图片和报告未改动。")); return true;
}
void MainWindow::objectAction(ObjectKind kind, const QString& operation, const QStringList& ids)
{
    if (busy_ || editingRegions_ || editingScene_ || managingObjects_) { setStatus(uiText("请等待当前截图或编辑完成。"), true); return; }
    if (configurationBlocked_) { setStatus(uiText("请先核对并应用当前配置。"), true); return; }
    if (operation == "edit") {
        if (ids.size() != 1) return;
        if (kind == ObjectKind::Region) {
            for (int row=0; row<regions_->rowCount(); ++row) if (regions_->item(row,1)->data(Qt::UserRole).toString()==ids.front()) regions_->setCurrentCell(row,1);
            editRegions();
        } else if (kind == ObjectKind::Scene) {
            for (int row=0; row<scenes_->count(); ++row) if (scenes_->item(row)->data(Qt::UserRole).toString()==ids.front()) scenes_->setCurrentRow(row);
            editScene(false);
        }
        return;
    }
    if (kind == ObjectKind::Scene && operation == "new") { editScene(true); return; }
    QScopedValueRollback<bool> guard(managingObjects_,true);
    Settings candidate=settings_; QString error;
    if (!collectSettings(candidate,error)) { setStatus(error,true); return; }
    auto rules=eventRules_; auto& profile=candidate.profiles[std::size_t(candidate.activeProfile)];
    QStringList names; RemovalSummary removal;
    if (operation == "delete") {
        if (ids.empty()) return;
        if (kind==ObjectKind::Region) { for (const auto& r:profile.regions) if(ids.contains(qs(r.id))) names.append(regionLabel(r)); removal=removeRegions(candidate,rules,qs(profile.id),ids); }
        else if (kind==ObjectKind::Scene) { for (const auto& s:candidate.scenes) if(ids.contains(qs(s.id))) names.append(qs(s.name)); removal=removeScenes(candidate,rules,ids); }
        else if (kind==ObjectKind::Profile) { for (const auto& p:candidate.profiles) if(ids.contains(qs(p.id))) names.append(qs(p.name)); removal=removeProfiles(candidate,rules,ids,uiText("默认配置")); }
        if (!removal.removed) return;
        QDialog confirm(this); confirm.setWindowTitle(uiText("删除选中的配置对象")); confirm.setObjectName("confirmObjectDeletion"); confirm.resize(540,400);
        auto* layout=new QVBoxLayout(&confirm); auto* heading=new QLabel(uiText("删除 %1 项？").arg(removal.removed)); layout->addWidget(heading);
        auto* objects=new QListWidget; objects->addItems(names); layout->addWidget(objects,1);
        auto* details=new QLabel(uiText("受影响场景：%1\n停用事件规则：%2\n历史图片、报告及已排队任务保持不变。").arg(removal.affectedScenes.join(" · ")).arg(removal.disabledRules)); details->setWordWrap(true); layout->addWidget(details);
        auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);layout->addWidget(buttons);
        buttons->button(QDialogButtonBox::Ok)->setText(uiText("删除选中"));buttons->button(QDialogButtonBox::Ok)->setObjectName("confirmDeleteObjects");buttons->button(QDialogButtonBox::Cancel)->setObjectName("cancelDeleteObjects");buttons->button(QDialogButtonBox::Cancel)->setDefault(true);
        connect(buttons,&QDialogButtonBox::accepted,&confirm,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&confirm,&QDialog::reject);
        if (confirm.exec()!=QDialog::Accepted) return;
    } else if (kind==ObjectKind::Region) {
        if (operation=="new") {
            const int number=profile.nextRegionNumber++;
            profile.regions.push_back({freshId(),uiText("区域 %1").arg(number).toStdString(),{0,0,640,400},true,number});
        } else if (operation=="duplicate") {
            if(ids.size()!=1) return;
            const auto found=std::find_if(profile.regions.begin(),profile.regions.end(),[&](const auto& r){return qs(r.id)==ids.front();}); if(found==profile.regions.end()) return;
            auto copy=*found; copy.id=freshId(); copy.name+=uiText(" 副本").toStdString(); copy.displayNumber=profile.nextRegionNumber++; profile.regions.push_back(copy);
        } else if(operation=="rename") {
            if(ids.size()!=1) return;
            const auto found=std::find_if(profile.regions.begin(),profile.regions.end(),[&](const auto& r){return qs(r.id)==ids.front();}); if(found==profile.regions.end()) return;
            bool ok{}; const auto name=QInputDialog::getText(this,uiText("重命名区域"),uiText("名称"),QLineEdit::Normal,qs(found->name),&ok); if(!ok||name.trimmed().isEmpty()) return; found->name=name.trimmed().toStdString();
        } else for(auto& r:profile.regions) if(ids.contains(qs(r.id))) r.enabled=operation=="enable";
        selectProfile(candidate,candidate.activeProfile);
    } else if(kind==ObjectKind::Scene) {
        if(operation=="rename" || operation=="duplicate") {
            if(ids.size()!=1) return;
            const auto found=std::find_if(candidate.scenes.begin(),candidate.scenes.end(),[&](const auto& s){return qs(s.id)==ids.front();}); if(found==candidate.scenes.end()) return;
            if(operation=="duplicate") { auto copy=*found; copy.id=freshId(); copy.name+=uiText(" 副本").toStdString(); copy.revision=1; copy.revisionCreatedAtUtc=QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs).toStdString(); candidate.scenes.push_back(copy); }
            else { bool ok{}; const auto name=QInputDialog::getText(this,uiText("重命名拼图"),uiText("名称"),QLineEdit::Normal,qs(found->name),&ok); if(!ok||name.trimmed().isEmpty()) return; found->name=name.trimmed().toStdString(); ++found->revision; found->revisionCreatedAtUtc=QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs).toStdString(); }
        } else {
            for(const auto& id:ids) {
                if(operation=="include" && std::none_of(profile.sceneIds.begin(),profile.sceneIds.end(),[&](const auto& s){return qs(s)==id;})) profile.sceneIds.push_back(id.toStdString());
                if(operation=="exclude") { std::erase_if(profile.sceneIds,[&](const auto& s){return qs(s)==id;}); if (profile.clipboardSelection=="scene:"+id.toStdString()) profile.clipboardSelection.clear(); }
            }
            profile.outputMode=profile.sceneIds.empty()?"originals":"both"; selectProfile(candidate,candidate.activeProfile);
        }
    } else if(kind==ObjectKind::Profile) {
        if(operation=="activate") {
            if(ids.size()!=1) return;
            for(int i=0;i<int(candidate.profiles.size());++i) if(qs(candidate.profiles[std::size_t(i)].id)==ids.front()) selectProfile(candidate,i);
        } else {
            const auto chosen=ids.empty()?qs(profile.id):ids.front();
            const auto found=std::find_if(candidate.profiles.begin(),candidate.profiles.end(),[&](const auto& p){return qs(p.id)==chosen;}); if(found==candidate.profiles.end()) return;
            bool ok{}; const auto name=QInputDialog::getText(this,operation=="rename"?uiText("重命名配置"):uiText("新建截图配置"),uiText("名称"),QLineEdit::Normal,operation=="new"?QString{}:qs(found->name)+(operation=="duplicate"?uiText(" 副本"):QString{}),&ok); if(!ok||name.trimmed().isEmpty()) return;
            if(operation=="rename") { found->name=name.trimmed().toStdString(); ++found->revision; }
            else { auto created=operation=="duplicate"?*found:emptyProfile(name.trimmed()); created.id=freshId(); created.name=name.trimmed().toStdString(); created.revision=1; candidate.profiles.push_back(created); selectProfile(candidate,int(candidate.profiles.size())-1); }
        }
    } else if(kind==ObjectKind::Output && operation=="exclude") {
        for(const auto& key:ids) {
            if(key.startsWith("scene:")) { const auto id=key.mid(6).toStdString(); std::erase_if(profile.sceneIds,[&](const auto& s){return s==id;}); if(profile.clipboardSelection==key.toStdString()) profile.clipboardSelection.clear(); }
            else for(auto& r:profile.regions) if(qs(r.id)==key) r.enabled=false;
        }
        if(profile.sceneIds.empty()) profile.outputMode="originals"; selectProfile(candidate,candidate.activeProfile);
    } else return;
    commitObjects(candidate,rules);
}
void MainWindow::setupObjectActions()
{
    auto bind=[this](QListWidget* list,ObjectKind kind) {
        list->setSelectionMode(QAbstractItemView::ExtendedSelection); list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        auto add=[this,list,kind](const QString& text,const QString& operation,const QKeySequence& shortcut=QKeySequence{}) {
            auto* action=new QAction(text,list); action->setObjectName(list->objectName()+"."+operation); list->addAction(action);
            if(!shortcut.isEmpty()) { action->setShortcut(shortcut); action->setShortcutContext(Qt::WidgetShortcut); }
            connect(action,&QAction::triggered,this,[this,list,kind,operation]{objectAction(kind,operation,selectedIds(list));}); return action;
        };
        add(uiText("新建"),"new",QKeySequence::New); add(uiText("编辑"),"edit"); add(uiText("复制为新对象"),"duplicate"); add(uiText("重命名"),"rename",QKeySequence(Qt::Key_F2));
        if(kind==ObjectKind::Region) {add(uiText("启用所选区域"),"enable");add(uiText("停用所选区域"),"disable");}
        if(kind==ObjectKind::Scene) {add(uiText("参与输出"),"include");add(uiText("取消输出"),"exclude");}
        add(uiText("删除选中"),"delete",QKeySequence::Delete);
        list->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(list,&QWidget::customContextMenuRequested,this,[this,list](const QPoint& pos){
            auto* item=list->itemAt(pos); if(item && !item->isSelected()) {list->clearSelection();list->setCurrentItem(item);item->setSelected(true);} else if(!item) list->clearSelection();
            const auto count=selectedIds(list).size(); QMenu menu(this); menu.addSection(uiText("已选 %1 项").arg(count));
            for(auto* a:list->actions()) {const auto op=a->objectName().section('.',-1);a->setEnabled(op=="new" || (count>0 && ((op!="edit"&&op!="rename"&&op!="duplicate")||count==1)));menu.addAction(a);}
            { QScopedValueRollback<bool> frozen(objectMenuOpen_,true); menu.exec(list->viewport()->mapToGlobal(pos)); }
            QTimer::singleShot(0,this,[this]{refreshWorkspace();});
        });
    };
    bind(sourceCards_,ObjectKind::Region);bind(overviewSources_,ObjectKind::Region);bind(scenes_,ObjectKind::Scene);
    batchCards_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(batchCards_,&QWidget::customContextMenuRequested,this,[this](const QPoint& pos){
        auto* item=batchCards_->itemAt(pos);if(item)batchCards_->setCurrentItem(item);QMenu menu(this);
        for(const auto& pair:std::vector<std::pair<QString,QString>>{{uiText("刷新批次"),"refreshBatchCards"},{uiText("打开批次目录"),"openBatchDirectory"},{uiText("用本批重排"),"recomposeSelectedBatch"},{uiText("将本批加入报告"),"appendSelectedBatch"},{uiText("查看 Word 任务 / 恢复核对"),"inspectReportQueue"}}) {
            auto* button=findChild<QPushButton*>(pair.second);auto* action=menu.addAction(pair.first);action->setEnabled(button && button->isEnabled() && (item || pair.second=="refreshBatchCards" || pair.second=="inspectReportQueue"));if(button)connect(action,&QAction::triggered,button,&QPushButton::click);
        }menu.exec(batchCards_->viewport()->mapToGlobal(pos));
    });
    batchImages_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(batchImages_,&QWidget::customContextMenuRequested,this,[this](const QPoint& pos){
        auto* item=batchImages_->itemAt(pos);if(!item)return;batchImages_->setCurrentItem(item);const auto path=item->data(Qt::UserRole).toString();QMenu menu(this);
        auto* copy=menu.addAction(uiText("复制图片"));connect(copy,&QAction::triggered,this,[this,path]{QImage image(path);if(image.isNull()){setStatus(uiText("图片无法读取。"),true);return;}QApplication::clipboard()->setImage(image);});
        auto* copyPath=menu.addAction(uiText("复制文件路径"));connect(copyPath,&QAction::triggered,this,[path]{QApplication::clipboard()->setText(path);});
        auto* folder=menu.addAction(uiText("打开批次目录"));connect(folder,&QAction::triggered,this,[path]{QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));});menu.exec(batchImages_->viewport()->mapToGlobal(pos));
    });
    reportCards_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(reportCards_,&QWidget::customContextMenuRequested,this,[this](const QPoint& pos){
        auto* item=reportCards_->itemAt(pos);const auto path=item?item->data(Qt::UserRole).toString():QString{};QMenu menu(this);
        auto* inspect=menu.addAction(uiText("查看 Word 任务 / 恢复核对"));connect(inspect,&QAction::triggered,this,[this]{showReportQueue();});
        auto* open=menu.addAction(uiText("打开报告"));open->setEnabled(QFileInfo(path).isFile());connect(open,&QAction::triggered,this,[path]{QDesktopServices::openUrl(QUrl::fromLocalFile(path));});menu.exec(reportCards_->viewport()->mapToGlobal(pos));
    });
    for(const auto& pair:std::vector<std::pair<QString,QString>>{{"deleteRegions","sourceCards.delete"},{"deleteScenes","enabledScenes.delete"}}) {
        auto* button=findChild<QPushButton*>(pair.first);auto* action=findChild<QAction*>(pair.second);if(button&&action)connect(action,&QAction::changed,button,[button,action]{button->setEnabled(action->isEnabled());});
    }

    for(auto* canvas:{topology_,overviewTopology_}) {
        canvas->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(canvas,&QWidget::customContextMenuRequested,this,[this,canvas](const QPoint& pos){
            const auto index=canvas->regionAt(pos); sourceCards_->clearSelection();
            if(index>=0 && index<sourceCards_->count()){sourceCards_->setCurrentRow(index);sourceCards_->item(index)->setSelected(true);}
            updateObjectActions();QMenu menu(this);menu.addSection(uiText("已选 %1 项").arg(selectedIds(sourceCards_).size()));menu.addActions(sourceCards_->actions());
            {QScopedValueRollback<bool> frozen(objectMenuOpen_,true);menu.exec(canvas->mapToGlobal(pos));}
            QTimer::singleShot(0,this,[this]{refreshWorkspace();});
        });
    }
    for (auto* list : {sourceCards_,overviewSources_,scenes_}) connect(list,&QListWidget::itemSelectionChanged,this,[this]{updateObjectActions();});
    overviewOutputs_->setSelectionMode(QAbstractItemView::ExtendedSelection); overviewOutputs_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(overviewOutputs_,&QListWidget::itemSelectionChanged,this,[this]{updateObjectActions();});
    auto* stop=new QAction(uiText("停止输出所选项"),overviewOutputs_);stop->setObjectName("overviewOutputs.exclude");overviewOutputs_->addAction(stop);
    connect(stop,&QAction::triggered,this,[this]{objectAction(ObjectKind::Output,"exclude",selectedIds(overviewOutputs_));});
    connect(overviewOutputs_,&QWidget::customContextMenuRequested,this,[this,stop](const QPoint& pos){
        auto* item=overviewOutputs_->itemAt(pos);if(item&&!item->isSelected()){overviewOutputs_->clearSelection();overviewOutputs_->setCurrentItem(item);}else if(!item)overviewOutputs_->clearSelection();
        const auto keys=selectedIds(overviewOutputs_);QMenu menu(this);menu.addSection(uiText("已选 %1 项").arg(keys.size()));
        auto add=[&](const QString& label,ObjectKind kind,const QString& operation,const QStringList& ids){auto* action=menu.addAction(label);connect(action,&QAction::triggered,this,[this,kind,operation,ids]{objectAction(kind,operation,ids);});};
        add(uiText("新建源区域"),ObjectKind::Region,"new",{});add(uiText("新建拼图"),ObjectKind::Scene,"new",{});
        if(!keys.empty()) {
            stop->setEnabled(true);menu.addAction(stop);
            const bool allScenes=std::all_of(keys.begin(),keys.end(),[](const auto& key){return key.startsWith("scene:");});
            const bool allRegions=std::none_of(keys.begin(),keys.end(),[](const auto& key){return key.startsWith("scene:");});
            if(allScenes||allRegions){QStringList ids;for(const auto& key:keys)ids.append(allScenes?key.mid(6):key);const auto kind=allScenes?ObjectKind::Scene:ObjectKind::Region;
                if(ids.size()==1){add(uiText("编辑"),kind,"edit",ids);add(uiText("重命名"),kind,"rename",ids);add(uiText("复制为新对象"),kind,"duplicate",ids);}
                add(allScenes?uiText("删除场景模板"):uiText("删除源区域"),kind,"delete",ids);
            }
        }
        {QScopedValueRollback<bool> frozen(objectMenuOpen_,true);menu.exec(overviewOutputs_->viewport()->mapToGlobal(pos));}
        QTimer::singleShot(0,this,[this]{refreshWorkspace();});
    });
}
void MainWindow::updateObjectActions()
{
    for (auto* list : {sourceCards_,overviewSources_,scenes_}) {
        const auto count=selectedIds(list).size();
        for(auto* action:list->actions()) {const auto op=action->objectName().section('.',-1);action->setEnabled(op=="new" || (count>0 && ((op!="edit"&&op!="rename"&&op!="duplicate")||count==1)));}
    }
    if(auto* label=findChild<QLabel*>("sourceSelectionCount"))label->setText(uiText("已选 %1 项").arg(selectedIds(sourceCards_).size()));
    if(auto* label=findChild<QLabel*>("sceneSelectionCount"))label->setText(uiText("已选 %1 项").arg(selectedIds(scenes_).size()));
    if(auto* label=findChild<QLabel*>("overviewSelectionCount"))label->setText(uiText("已选来源 %1 项 · 输出 %2 项").arg(selectedIds(overviewSources_).size()).arg(selectedIds(overviewOutputs_).size()));
}
void MainWindow::showProfileManager()
{
    QDialog dialog(this);dialog.setWindowTitle(uiText("管理截图配置"));dialog.setObjectName("profileManager");dialog.resize(540,420);
    auto* layout=new QVBoxLayout(&dialog);auto* list=new QListWidget;list->setObjectName("managedProfiles");list->setSelectionMode(QAbstractItemView::ExtendedSelection);layout->addWidget(list);
    auto* count=new QLabel;layout->addWidget(count);auto showCount=[list,count]{count->setText(uiText("已选 %1 项").arg(selectedIds(list).size()));};connect(list,&QListWidget::itemSelectionChanged,&dialog,showCount);
    auto refill=[this,list]{const auto ids=selectedIds(list);list->clear();for(const auto& p:settings_.profiles){auto* item=new QListWidgetItem(qs(p.name)+uiText(" · %1 个区域").arg(p.regions.size()),list);item->setData(Qt::UserRole,qs(p.id));item->setSelected(ids.contains(qs(p.id)));}if(list->selectedItems().empty())list->setCurrentRow(settings_.activeProfile);};refill();
    auto* row=new QHBoxLayout;layout->addLayout(row);
    const std::vector<std::pair<QString,QString>> operations{{uiText("新建"),"new"},{uiText("复制"),"duplicate"},{uiText("重命名"),"rename"},{uiText("切换"),"activate"},{uiText("删除选中"),"delete"}};
    for(const auto& op:operations){auto* button=new QPushButton(op.first);button->setObjectName("profiles."+op.second);row->addWidget(button);auto* action=new QAction(op.first,list);action->setObjectName("managedProfiles."+op.second);list->addAction(action);connect(action,&QAction::triggered,&dialog,[this,list,op,refill]{objectAction(ObjectKind::Profile,op.second,selectedIds(list));refill();});connect(button,&QPushButton::clicked,action,&QAction::trigger);connect(action,&QAction::changed,button,[button,action]{button->setEnabled(action->isEnabled());});}
    auto enableActions=[list]{const auto n=selectedIds(list).size();for(auto* a:list->actions()){const auto op=a->objectName().section('.',-1);a->setEnabled(op=="new" || (op=="delete"?n>0:n==1));}};
    for(auto* action:list->actions()){const auto op=action->objectName().section('.',-1);if(op=="delete")action->setShortcut(QKeySequence::Delete);if(op=="rename")action->setShortcut(QKeySequence(Qt::Key_F2));if(op=="new")action->setShortcut(QKeySequence::New);action->setShortcutContext(Qt::WidgetShortcut);}
    connect(list,&QListWidget::itemSelectionChanged,&dialog,enableActions);enableActions();showCount();
    list->setContextMenuPolicy(Qt::ActionsContextMenu);auto* close=new QDialogButtonBox(QDialogButtonBox::Close);layout->addWidget(close);connect(close,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);dialog.exec();
}
} // namespace aitsnap
