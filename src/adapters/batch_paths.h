// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QDir>
#include <QFileInfo>
#include <QJsonObject>
namespace aitsnap {
// Flat storage has a fixed relative structure, never a JSON-supplied absolute root.
inline QString batchImageRoot(const QString& manifest,const QJsonObject& object){
    auto directory=QFileInfo(manifest).absoluteDir();
    const auto mode=object["extensions"].toObject()["storageLayout"].toString("batch");
    if(mode=="batch")return directory.canonicalPath();
    if(mode!="flat" || QFileInfo(manifest).fileName()!="batch.json")return {};
    if(!directory.cdUp() || directory.dirName()!="batches" || !directory.cdUp() || directory.dirName()!=".aitsnap" || !directory.cdUp())return {};
    return directory.canonicalPath();
}
inline QString batchCaptureRoot(const QString& manifest,const QJsonObject& object){
    const auto imageRoot=batchImageRoot(manifest,object);if(imageRoot.isEmpty())return {};
    return object["extensions"].toObject()["storageLayout"]=="flat"?imageRoot:QFileInfo(imageRoot).absolutePath();
}
inline QString batchArtifactPath(const QString& manifest,const QJsonObject& object,const QJsonObject& artifact){
    const auto root=batchImageRoot(manifest,object);if(root.isEmpty())return {};
    const auto relative=QDir::fromNativeSeparators(artifact["relativePath"].toString());
    if(!relative.isEmpty() && (QDir::isAbsolutePath(relative) || relative.contains(':') || relative.split('/').contains("..")))return {};
    const auto path=QFileInfo(relative.isEmpty()?artifact["path"].toString():QDir(root).filePath(relative)).canonicalFilePath();
    const auto prefix=root.endsWith('/')?root:root+'/';
    return !path.isEmpty() && path.startsWith(prefix,Qt::CaseInsensitive)?path:QString{};
}
}
