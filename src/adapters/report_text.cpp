// SPDX-License-Identifier: Apache-2.0
#include "report_text.h"
#include "desktop/appearance.h"
#include <QStringList>
namespace aitsnap {
static QString qs(const std::string& s){return QString::fromStdString(s);}
QMap<QString,QString> reportTextValues(const core::CaptureRequest& request)
{
    const auto& c=request.context;
    return {{"profile",qs(request.profile.name)},{"datetime",qs(request.issuedAtUtc)},{"sequence",request.captureSequence>0?QString::number(request.captureSequence):QString{}},
        {"test",qs(c.testId)},{"device",qs(c.deviceId)},{"station",qs(c.stationId)},{"operator",qs(c.operatorId)},
        {"run",qs(c.sequenceRunId)},{"step",qs(c.stepExecutionId)},{"command",qs(c.commandInvocationId)},{"event",qs(c.eventId)}};
}
QString expandReportText(const QString& pattern,const QMap<QString,QString>& values,QString& error)
{
    error.clear();if(pattern.size()>4096){error=uiText("报告文字模板最多 4096 个字符。");return {};}
    QString result;
    for(int i=0;i<pattern.size();){
        const auto c=pattern[i];
        if((c=='{' || c=='}') && i+1<pattern.size() && pattern[i+1]==c){result+=c;i+=2;continue;}
        if(c=='{'){
            const auto end=pattern.indexOf('}',i+1);if(end<0){error=uiText("报告模板有未闭合的花括号，位置 %1。").arg(i+1);return {};}
            const auto token=pattern.mid(i+1,end-i-1);
            if(!values.contains(token)){error=uiText("报告模板有未知字段 {%1}，位置 %2。").arg(token).arg(i+1);return {};}
            result+=values[token];i=int(end)+1;
        }else if(c=='}'){error=uiText("报告模板有多余的右花括号，位置 %1。").arg(i+1);return {};}
        else{result+=c;++i;}
    }
    return result;
}
bool validateReportText(const ReportTextSpec& spec,QString& error)
{
    for(const auto& text:{spec.titleTemplate,spec.captionTemplate,spec.contextTemplate})if(text.size()>4096){error=uiText("报告文字模板最多 4096 个字符。");return false;}
    auto values=reportTextValues({});
    if(spec.titleEnabled){expandReportText(spec.titleTemplate,values,error);if(!error.isEmpty())return false;}
    if(spec.contextEnabled){expandReportText(spec.contextTemplate,values,error);if(!error.isEmpty())return false;}
    values.insert("name",{});values.insert("label",{});values.insert("number",{});values.insert("kind",{});values.insert("scene_revision",{});
    if(spec.captionEnabled){expandReportText(spec.captionTemplate,values,error);if(!error.isEmpty())return false;}
    error.clear();return true;
}
QJsonObject reportTextJson(const ReportTextSpec& spec)
{
    return {{"version",1},{"preset",spec.preset},{"titleEnabled",spec.titleEnabled},{"captionEnabled",spec.captionEnabled},{"contextEnabled",spec.contextEnabled},
        {"titleTemplate",spec.titleTemplate},{"captionTemplate",spec.captionTemplate},{"contextTemplate",spec.contextTemplate}};
}
bool reportTextFromJson(const QJsonObject& value,ReportTextSpec& spec,QString& error)
{
    if(value.isEmpty())return true;
    if(value["version"].toInt()!=1){error="Unsupported report text version";return false;}
    for(const auto* flag:{"titleEnabled","captionEnabled","contextEnabled"})if(!value[flag].isBool()){error="Invalid report text switches";return false;}
    for(const auto* field:{"titleTemplate","captionTemplate","contextTemplate"})if(!value[field].isString() || value[field].toString().size()>4096){error="Invalid report text template";return false;}
    spec.preset=value["preset"].toString("custom");spec.titleEnabled=value["titleEnabled"].toBool();spec.captionEnabled=value["captionEnabled"].toBool();spec.contextEnabled=value["contextEnabled"].toBool();
    spec.titleTemplate=value["titleTemplate"].toString();spec.captionTemplate=value["captionTemplate"].toString();spec.contextTemplate=value["contextTemplate"].toString();
    return true;
}
}
