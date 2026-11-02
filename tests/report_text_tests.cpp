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
        auto composite=output;composite.kind="COMPOSITE";composite.regionId.clear();composite.sceneId="scene";composite.sceneRevision=3;
        input.batch.outputs.push_back(composite);core::SceneTemplate scene;scene.id="scene";scene.name="拼图甲";scene.revision=3;input.sceneSnapshots={scene};
        for(const auto& layout:{"inline","table"})for(int flags=0;flags<8;++flags){
            ReportOptions matrix;matrix.documentPath=temp.filePath("matrix.docx");matrix.layout=layout;matrix.text.titleEnabled=bool(flags&1);matrix.text.captionEnabled=bool(flags&2);matrix.text.contextEnabled=bool(flags&4);
            matrix.text.titleTemplate="标题 {sequence}";matrix.text.captionTemplate="{name}|{label}|{number}|{kind}|{scene_revision}";matrix.text.contextTemplate="测试 {test}";
            const auto generated=makeReportJob(input,matrix,"append",error);check(!generated.isEmpty() && validateReportJob(generated,error),"all text switch combinations yield valid frozen jobs for inline and table");
            const auto generatedExt=generated["extensions"].toObject();QStringList expected;if(flags&1)expected<<"标题 17";if(flags&4)expected<<"测试 试验A";check(generatedExt["heading"]==expected.join(" | "),"only enabled title/context contributes visible text");
            const auto generatedItems=generated["items"].toArray();check(generatedItems.size()==2 && generatedItems[0].toObject()["renderAs"]==(QString(layout)=="table"?"table_cell":"inline_image"),"both image types preserve selected layout");
            check(generatedExt["textPolicy"].toObject()["captions"].toBool()==bool(flags&2),"caption switch freezes into helper policy");
            if(flags&2){check(generatedItems[0].toObject()["caption"]=="renamed|#3 renamed|3|ORIGINAL|","original-only caption fields");check(generatedItems[1].toObject()["caption"]=="拼图甲|拼图甲 r3||COMPOSITE|3","composite caption fields match help contract");}
            else check(generatedItems[0].toObject()["caption"].toString().isEmpty() && generatedItems[1].toObject()["caption"].toString().isEmpty(),"disabled captions empty for all outputs");
        }
        ReportTextSpec invalid;invalid.titleTemplate="{datetime:local}";check(!validateReportText(invalid,error),"unsupported time formatting is a clear unknown-field error");invalid.titleTemplate=QString(4097,'x');check(!validateReportText(invalid,error),"template size limit");
        auto settings=defaultSettings();settings.profiles={input.batch.request.profile};settings.report.text=options.text;const auto settingsPath=temp.filePath("settings.json");check(saveSettings(settingsPath,settings,error),"save report settings");Settings restored;check(loadSettings(settingsPath,restored,error) && restored.report.text.titleTemplate=="new" && !restored.report.text.contextEnabled,"text settings roundtrip");
        const auto exportIndex=app.arguments().indexOf("--export-native-fixtures");
        if(exportIndex>=0){
            check(exportIndex+1<app.arguments().size(),"native fixture destination required");const auto root=QDir(app.arguments()[exportIndex+1]).absolutePath();check(!QDir(root).exists() && QDir().mkpath(root),"new isolated native fixture directory only");
            check(saveImageNew(image,root+"/original.png","PNG",error),"native original fixture");QImage blue(24,20,QImage::Format_RGB32);blue.fill(Qt::blue);check(saveImageNew(blue,root+"/composite.png","PNG",error),"native composite fixture");
            input.directory=root;for(auto& item:input.batch.outputs){const auto fileName=item.kind=="ORIGINAL"?"original.png":"composite.png";item.path=(root+'/'+fileName).toStdString();QFile f(QString::fromStdString(item.path));check(f.open(QIODevice::ReadOnly),"native source hash");item.sha256=QCryptographicHash::hash(f.readAll(),QCryptographicHash::Sha256).toHex().toStdString();}
            QJsonArray cases;
            for(const auto& layout:{"inline","table"})for(const auto& preset:{"images","title","caption","custom"}){
                const auto key=QString(layout)+'-'+preset;ReportOptions native;native.documentPath=root+'/'+key+".docx";native.layout=layout;
                native.text.titleEnabled=QString(preset)=="title" || QString(preset)=="custom";native.text.captionEnabled=QString(preset)=="caption" || QString(preset)=="custom";native.text.contextEnabled=QString(preset)=="custom";
                native.text.titleTemplate="批次标题 {sequence}\n{{固定}} {test}";native.text.captionTemplate="图片说明 {kind}:{name}|{number}|{scene_revision}";native.text.contextTemplate="上下文 {test} \"quoted\"";QJsonArray jobs;
                for(int step=0;step<2;++step){input.batch.batchId=(key+QString::number(step)).toStdString();input.batch.request.captureSequence=21+step;auto exported=makeReportJob(input,native,step?"append":"create",error);check(!exported.isEmpty(),"new Word text fixture valid");exported["state"]="RUNNING";auto ext=exported["extensions"].toObject();ext["dispatchId"]=exported["jobId"];exported["extensions"]=ext;const auto jobPath=root+'/'+key+QString("-%1.job.json").arg(step);check(storeReportJob(jobPath,exported,error),"export isolated native fixture job");jobs.append(jobPath);}
                cases.append(QJsonObject{{"name",key},{"layout",layout},{"preset",preset},{"target",native.documentPath},{"jobs",jobs}});
            }
            check(saveBytesNew(QJsonDocument(QJsonObject{{"scope","NEW 0.8.3 synthetic Word text tests; no existing user documents"},{"cases",cases}}).toJson(),root+"/matrix.json",error),"export native matrix contract");
        }
        std::cout<<"REPORT_TEXT_PASS templates/escapes/snapshot/image-only/old-fingerprint/policy-tamper/settings; native Word NOT_RUN\n";return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
