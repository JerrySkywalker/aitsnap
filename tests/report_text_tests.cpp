// SPDX-License-Identifier: Apache-2.0
#include "adapters/report_queue.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <iostream>
#include <stdexcept>
using namespace aitsnap;
static void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
int main(int argc,char** argv)
{
    QCoreApplication app(argc,argv);
    try{
        QString error;core::CaptureRequest request;request.profile.name="配置{literal}";request.issuedAtUtc="2026-11-01T15:00:00.000Z";request.captureSequence=17;request.context.testId="试验A";
        const auto values=reportTextValues(request);
        check(expandReportText("{{固定}}\n{profile} / {sequence} / {test} / {event}",values,error)=="{固定}\n配置{literal} / 17 / 试验A / " && error.isEmpty(),"literal Unicode multiline missing values no recursive expansion");
        check(expandReportText("{unknown}",values,error).isEmpty() && !error.isEmpty(),"unknown token rejected");
        check(expandReportText("x}",values,error).isEmpty() && !error.isEmpty(),"unmatched bracket rejected");
        ReportTextSpec spec;spec.titleTemplate="{name}";check(!validateReportText(spec,error),"item fields not valid in batch title");spec.titleEnabled=false;check(validateReportText(spec,error),"disabled text not evaluated");
        QTemporaryDir temp;check(temp.isValid(),"temporary report fixture");QImage image(32,16,QImage::Format_RGB32);image.fill(Qt::red);const auto path=temp.filePath("source.png");check(saveImageNew(image,path,"PNG",error),"save fixture");
        QFile source(path);check(source.open(QIODevice::ReadOnly),"read fixture");const auto bytes=source.readAll();
        CaptureOutcome input;input.directory=temp.path();input.batch.batchId="text-batch";input.batch.request=request;input.batch.request.profile.id="text-profile";input.batch.request.profile.regions.push_back({"r","电压",{0,0,32,16},true,3});
        core::OutputResult output{"ORIGINAL",path.toStdString(),"SAVED",{}};output.regionId="r";output.sha256=QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex().toStdString();input.batch.outputs={output};
        ReportOptions options;options.documentPath=temp.filePath("result.docx");
        auto job=makeReportJob(input,options,"create",error);check(!job.isEmpty(),"legacy defaults create job");
        check(job["items"].toArray()[0].toObject()["caption"]==QString("#3 电压 | ")+QString::fromStdString(request.issuedAtUtc),"default captions retain readable existing format");
        const auto ext=job["extensions"].toObject();check(ext["heading"].toString().startsWith("AITSnap 配置{literal} | ") && ext["heading"].toString().contains("test=试验A"),"default heading retains context");
        auto legacy=job;auto oldExt=legacy["extensions"].toObject();oldExt.remove("textPolicy");legacy["extensions"]=oldExt;
        const QJsonObject stable{{"jobId",legacy["jobId"]},{"batchId",legacy["batchId"]},{"target",legacy["target"]},{"items",legacy["items"]},{"sources",oldExt["sources"]},{"layout",oldExt["layout"]},{"heading",oldExt["heading"]}};
        const auto oldFingerprint=QString::fromLatin1(QCryptographicHash::hash(QJsonDocument(stable).toJson(QJsonDocument::Compact),QCryptographicHash::Sha256).toHex());
        check(reportFingerprint(legacy)==oldFingerprint,"old fingerprint algorithm unchanged");oldExt["fingerprint"]=oldFingerprint;legacy["extensions"]=oldExt;check(validateReportJob(legacy,error),"old jobs readable unchanged");
        options.text.titleEnabled=false;options.text.contextEnabled=false;options.text.captionEnabled=false;
        auto imageOnly=makeReportJob(input,options,"append",error);check(!imageOnly.isEmpty() && imageOnly["extensions"].toObject()["heading"].toString().isEmpty() && imageOnly["items"].toArray()[0].toObject()["caption"].toString().isEmpty(),"image-only freezes no visible text");
        options.text.titleEnabled=true;options.text.titleTemplate="固定标题\n{test}";options.text.captionEnabled=true;options.text.captionTemplate="{number}: {name} / {kind} / {sequence}";
        auto custom=makeReportJob(input,options,"append",error);check(custom["extensions"].toObject()["heading"]=="固定标题\n试验A" && custom["items"].toArray()[0].toObject()["caption"]=="3: 电压 / ORIGINAL / 17","custom text resolved from capture snapshot");
        const auto frozen=QJsonDocument(custom).toJson();options.text.titleTemplate="new";input.batch.request.profile.regions.front().name="renamed";check(QJsonDocument(custom).toJson()==frozen,"queued resolved instructions remain frozen");
        auto tampered=custom;auto policyExt=tampered["extensions"].toObject();auto policy=policyExt["textPolicy"].toObject();policy["captions"]=false;policyExt["textPolicy"]=policy;tampered["extensions"]=policyExt;check(!validateReportJob(tampered,error),"changing visible text policy invalidates immutable job");
        check(source.seek(0) && source.readAll()==bytes,"source pixels unchanged");
        auto settings=defaultSettings();settings.profiles={input.batch.request.profile};settings.report.text=options.text;const auto settingsPath=temp.filePath("settings.json");check(saveSettings(settingsPath,settings,error),"save report settings");Settings restored;check(loadSettings(settingsPath,restored,error) && restored.report.text.titleTemplate=="new" && !restored.report.text.contextEnabled,"text settings roundtrip");
        std::cout<<"REPORT_TEXT_PASS templates/escapes/snapshot/image-only/old-fingerprint/policy-tamper/settings; native Word NOT_RUN\n";return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
