// SPDX-License-Identifier: Apache-2.0
#include "main_window.h"
#include "desktop/appearance.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QSignalBlocker>
#include <QTimer>
#include <QTableWidget>
#include <algorithm>
namespace aitsnap {
void MainWindow::buildReportTextUi(QFormLayout* form)
{
    reportTextPreset_=new QComboBox;reportTextPreset_->setObjectName("reportTextPreset");
    reportTextPreset_->addItem(uiText("保持原有图文"),"legacy");reportTextPreset_->addItem(uiText("仅名称"),"name");reportTextPreset_->addItem(uiText("名称与时间"),"time");reportTextPreset_->addItem(uiText("自定义文字"),"custom");reportTextPreset_->addItem(uiText("只写图片"),"images");
    reportTextPreset_->setCurrentIndex(std::max(0,reportTextPreset_->findData(settings_.report.text.preset)));form->addRow(uiText("写入文字"),reportTextPreset_);
    const auto& text=settings_.report.text;const bool flags[]{text.titleEnabled,text.captionEnabled,text.contextEnabled};const QString templates[]{text.titleTemplate,text.captionTemplate,text.contextTemplate};
    const QStringList labels{uiText("批次标题"),uiText("图片说明"),uiText("上下文信息")};
    for(int i=0;i<3;++i){reportTextEnabled_[i]=new QCheckBox(labels[i]);reportTextEnabled_[i]->setChecked(flags[i]);reportTextEnabled_[i]->setObjectName(QString("reportTextEnabled%1").arg(i));form->addRow(reportTextEnabled_[i]);
        reportTextTemplates_[i]=new QPlainTextEdit(templates[i]);reportTextTemplates_[i]->setObjectName(QString("reportTextTemplate%1").arg(i));reportTextTemplates_[i]->setMaximumHeight(85);reportTextTemplates_[i]->setEnabled(flags[i]);form->addRow(reportTextTemplates_[i]);}
    auto* fields=new QLabel(uiText("可直接输入固定文字。字段：{profile} {datetime} {sequence} {test} {device} {station} {operator} {run} {step} {command} {event}；图片说明还支持 {name} {label} {number} {kind} {scene_revision}。{{ 和 }} 表示字面花括号。"));fields->setWordWrap(true);form->addRow(fields);
    reportTextPreview_=new QLabel;reportTextPreview_->setObjectName("reportTextPreview");reportTextPreview_->setWordWrap(true);reportTextPreview_->setTextFormat(Qt::PlainText);reportTextPreview_->setTextInteractionFlags(Qt::TextSelectableByMouse);form->addRow(uiText("文字预览"),reportTextPreview_);
    auto custom=[this]{const QSignalBlocker block(reportTextPreset_);reportTextPreset_->setCurrentIndex(reportTextPreset_->findData("custom"));refreshReportTextPreview();};
    for(int i=0;i<3;++i){connect(reportTextTemplates_[i],&QPlainTextEdit::textChanged,this,custom);connect(reportTextEnabled_[i],&QCheckBox::toggled,this,[this,i,custom](bool on){reportTextTemplates_[i]->setEnabled(on);custom();});}
    connect(reportTextPreset_,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[this]{
        const auto preset=reportTextPreset_->currentData().toString();if(preset=="custom"){refreshReportTextPreview();return;}
        ReportTextSpec next;
        if(preset!="legacy"){next.titleEnabled=false;next.contextEnabled=false;next.captionEnabled=preset!="images";next.captionTemplate=preset=="name"?"{name}":"{name} | {datetime}";}
        const bool flags[]{next.titleEnabled,next.captionEnabled,next.contextEnabled};const QString templates[]{next.titleTemplate,next.captionTemplate,next.contextTemplate};
        for(int i=0;i<3;++i){const QSignalBlocker a(reportTextEnabled_[i]),b(reportTextTemplates_[i]);reportTextEnabled_[i]->setChecked(flags[i]);reportTextTemplates_[i]->setEnabled(flags[i]);reportTextTemplates_[i]->setPlainText(templates[i]);}refreshReportTextPreview();});
    QTimer::singleShot(0,this,[this]{refreshReportTextPreview();});
}
void MainWindow::refreshReportTextPreview()
{
    if(!reportTextPreview_ || settings_.profiles.empty())return;
    const auto spec=selectedReportOptions().text;QString error;if(!validateReportText(spec,error)){reportTextPreview_->setText(error);return;}
    core::CaptureRequest request;request.profile=settings_.profiles[std::size_t(settings_.activeProfile)];request.profile.name=profileName_->text().toStdString();request.captureSequence=settings_.sequence+1;request.issuedAtUtc=QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs).toStdString();request.context=selectedContext_;
    if(selectedContextSnapshot_.isEmpty()){request.context.testId=testId_?testId_->text().toStdString():settings_.testId.toStdString();request.context.deviceId=device_?device_->text().toStdString():settings_.device.toStdString();}
    request.context.stationId=station_?station_->text().toStdString():settings_.station.toStdString();request.context.operatorId=operator_?operator_->text().toStdString():settings_.operatorName.toStdString();
    auto values=reportTextValues(request);QStringList lines;
    if(spec.titleEnabled)lines<<expandReportText(spec.titleTemplate,values,error);if(spec.contextEnabled)lines<<expandReportText(spec.contextTemplate,values,error);
    values["name"]=request.profile.regions.empty()?uiText("区域"):QString::fromStdString(request.profile.regions.front().name);values["number"]=request.profile.regions.empty()?"1":QString::number(request.profile.regions.front().displayNumber);values["label"]="#"+values["number"]+" "+values["name"];values["kind"]="ORIGINAL";values["scene_revision"]=QString{};
    if(regions_ && regions_->rowCount()>0){const auto* row=regions_->item(std::max(0,regions_->currentRow()),1);if(row){values["name"]=row->text();values["number"]=QString::number(row->data(Qt::UserRole+1).toInt());values["label"]="#"+values["number"]+" "+values["name"];}}
    if(spec.captionEnabled)lines<<expandReportText(spec.captionTemplate,values,error);
    reportTextPreview_->setText(lines.empty()?uiText("只写入图片，不插入可见文字。"):lines.join('\n'));
}
}
