/*
    CloudCross: Opensource program for syncronization of local files and folders with Google Drive

    Copyright (C) 2016  Vladimir Kamensky
    Copyright (C) 2016  Master Soft LLC.

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation version 2
    of the License.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include "msgoogledrive.h"
#include "include/msrequest.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>




MSGoogleDrive::MSGoogleDrive() :
    MSCloudProvider()
{
    this->providerName="GoogleDrive";
    this->tokenFileName=".grive";
    this->stateFileName=".grive_state";
    this->trashFileName=".trash";

}

//=======================================================================================

bool MSGoogleDrive::auth(){

    MSRequest* req=new MSRequest();

    req->setRequestUrl("https://accounts.google.com/o/oauth2/v2/auth");
    req->setMethod("get");

    req->addQueryItem("scope",                  "https://www.googleapis.com/auth/drive+https://www.googleapis.com/auth/userinfo.email+https://www.googleapis.com/auth/userinfo.profile+https://docs.google.com/feeds/+https://docs.googleusercontent.com/+https://spreadsheets.google.com/feeds/");
    req->addQueryItem("redirect_uri",           "urn:ietf:wg:oauth:2.0:oob");
    req->addQueryItem("response_type",          "code");
    req->addQueryItem("client_id",              "834415955748-oq0p2m5dro2bvh3bu0o5bp19ok3qrs3f.apps.googleusercontent.com");
    req->addQueryItem("access_type",            "offline");
    req->addQueryItem("approval_prompt",        "force");
    req->addQueryItem("state",                  "1");

    req->exec();


    if(!req->replyOK()){
        req->printReplyError();
        exit(1);
    }

    if(!this->testReplyBodyForError(req->readReplyText())){
        qStdOut()<< "Service error. " << this->getReplyErrorString(req->readReplyText()) << endl;
        exit(0);
    }


    qStdOut()<<"-------------------------------------"<<endl;
    qStdOut()<< QObject::tr("Please go to this URL and get an authentication code:\n")<<endl;

    qStdOut() << req->replyURL;//lastReply->url().toString();
    qStdOut()<<""<<endl;
    qStdOut()<<"-------------------------------------"<<endl;
    qStdOut()<<QObject::tr("Please input the authentication code here: ")<<endl;


    QTextStream s(stdin);
    QString authCode = s.readLine();

    delete(req);

    req=new MSRequest();

    req->setRequestUrl("https://accounts.google.com/o/oauth2/token");
    req->setMethod("post");

    req->addQueryItem("client_id",          "834415955748-oq0p2m5dro2bvh3bu0o5bp19ok3qrs3f.apps.googleusercontent.com");
    req->addQueryItem("client_secret",      "YMBWydU58CvF3UP9CSna-BwS");
    req->addQueryItem("code",               authCode.trimmed());
    req->addQueryItem("grant_type",         "authorization_code");
    req->addQueryItem("redirect_uri",       "urn:ietf:wg:oauth:2.0:oob");

    req->exec();


    if(!req->replyOK()){
        req->printReplyError();
        exit(1);
    }

    if(!this->testReplyBodyForError(req->readReplyText())){
        qStdOut()<< "Service error. " << this->getReplyErrorString(req->readReplyText()) << endl;
        exit(0);
    }


    QString content= req->replyText;//lastReply->readAll();

    //qStdOut() << content;

    QJsonDocument json = QJsonDocument::fromJson(content.toUtf8());
    QJsonObject job = json.object();
    QString v=job["refresh_token"].toString();

    if(v!=""){

        this->token=v;
        qStdOut() << "Token was succesfully accepted and saved. To start working with the program run ccross without any options for start full synchronize."<<endl;
        return true;
    }
    else{
        return false;
    }

}

//=======================================================================================

void MSGoogleDrive::saveTokenFile(QString path){

        QFile key(path+"/"+this->tokenFileName);
        key.open(QIODevice::WriteOnly | QIODevice::Text);
        QTextStream outk(&key);
        outk << "{\"refresh_token\" : \""+this->token+"\"}";
        key.close();
}

//=======================================================================================

bool MSGoogleDrive::loadTokenFile(QString path){

    QFile key(path+"/"+this->tokenFileName);

    if(!key.open(QIODevice::ReadOnly))
    {
        qStdOut() << "Access key missing or corrupt. Start CloudCross with -a option for obtained private key."  <<endl;
        return false;
    }

    QTextStream instream(&key);
    QString line;
    while(!instream.atEnd()){

        line+=instream.readLine();
    }

    QJsonDocument json = QJsonDocument::fromJson(line.toUtf8());
    QJsonObject job = json.object();
    QString v=job["refresh_token"].toString();

    this->token=v;

    key.close();
    return true;

}

//=======================================================================================

void MSGoogleDrive::loadStateFile(){

    QFile key(this->workPath+"/"+this->stateFileName);

    if(!key.open(QIODevice::ReadOnly))
    {
        qStdOut() << "Previous state file not found. Start in stateless mode."<<endl ;
        return;
    }

    QTextStream instream(&key);
    QString line;
    while(!instream.atEnd()){

        line+=instream.readLine();
    }

    QJsonDocument json = QJsonDocument::fromJson(line.toUtf8());
    QJsonObject job = json.object();

    this->lastSyncTime=QJsonValue(job["last_sync"].toObject()["sec"]).toVariant().toULongLong();

    key.close();
    return;

}

//=======================================================================================

bool MSGoogleDrive::refreshToken(){

    MSRequest* req=new MSRequest();

    req->setRequestUrl("https://www.googleapis.com/oauth2/v4/token");
    req->setMethod("post");

    req->addQueryItem("refresh_token",          this->token);
    req->addQueryItem("client_id",              "834415955748-oq0p2m5dro2bvh3bu0o5bp19ok3qrs3f.apps.googleusercontent.com");
    req->addQueryItem("client_secret",          "YMBWydU58CvF3UP9CSna-BwS");
    req->addQueryItem("grant_type",             "refresh_token");

    req->exec();


    if(!req->replyOK()){
        req->printReplyError();
        exit(1);
    }

    if(!this->testReplyBodyForError(req->readReplyText())){
        qStdOut()<< "Service error. " << this->getReplyErrorString(req->readReplyText()) << endl;
        exit(0);
    }


    QString content= req->readReplyText();

    QJsonDocument json = QJsonDocument::fromJson(content.toUtf8());
    QJsonObject job = json.object();
    QString v=job["access_token"].toString();

    if(v!=""){
       this->access_token=v;
        delete(req);
        return true;
    }
    else{
        delete(req);
        return false;
    }

}

//=======================================================================================

void MSGoogleDrive::createHashFromRemote(){

    MSRequest* req=new MSRequest();

    req->setRequestUrl("https://www.googleapis.com/drive/v2/files");
    req->setMethod("get");

    req->addQueryItem("access_token",           this->access_token);

    req->exec();


    if(!req->replyOK()){
        req->printReplyError();
        exit(1);
    }

    if(!this->testReplyBodyForError(req->readReplyText())){
        qStdOut()<< "Service error. " << this->getReplyErrorString(req->readReplyText()) << endl;
        exit(0);
    }


    QString list=req->readReplyText();


    // collect all files in GoogleDrive to array driveJSONFileList
    QJsonDocument jsonList = QJsonDocument::fromJson(list.toUtf8());
    QJsonObject job = jsonList.object();

    do{

        delete(req);

        req=new MSRequest();


        req->setRequestUrl("https://www.googleapis.com/drive/v2/files");
        req->setMethod("get");

        req->addQueryItem("access_token",           this->access_token);
        req->addQueryItem("maxResults",           "1000");

        QString nextPageToken=job["nextPageToken"].toString();

        QJsonArray items=job["items"].toArray();

        for(int i=0;i<items.size();i++){

            driveJSONFileList.insert(items[i].toObject()["id"].toString(),items[i]);

        }

        req->addQueryItem("pageToken",           nextPageToken);

        req->exec();


        if(!req->replyOK()){
            req->printReplyError();
            exit(1);
        }

        if(!this->testReplyBodyForError(req->readReplyText())){
            qStdOut()<< "Service error. " << this->getReplyErrorString(req->readReplyText()) << endl;
            exit(0);
        }


        list=req->readReplyText();

        jsonList = QJsonDocument::fromJson(list.toUtf8());



        job = jsonList.object();

    }while(job["nextPageToken"].toString()!=""); //while(false);

    delete(req);
}


//=======================================================================================

QString MSGoogleDrive::getRoot(){

    QHash<QString,QJsonValue>::iterator i=driveJSONFileList.begin();
    QJsonValue v;

    QHash<QString,QJsonValue> out;

    while(i != driveJSONFileList.end()){

        v= i.value();

        bool isRoot=v.toObject()["parents"].toArray()[0].toObject()["isRoot"].toBool();

        if(isRoot){

            return v.toObject()["parents"].toArray()[0].toObject()["id"].toString();

        }

       i++;

    }

    return "";

}


//=======================================================================================

QHash<QString,QJsonValue> MSGoogleDrive::get(QString parentId, int target){

    bool files=target & 1;
    bool folders=target & 2;
    bool foldersAndFiles =false;
    bool noTrash = target & 4;

    if(folders && files){
        foldersAndFiles=true;
    }


    QHash<QString,QJsonValue>::iterator i=driveJSONFileList.begin();
    QJsonValue v;

    QHash<QString,QJsonValue> out;

    while(i != driveJSONFileList.end()){

        v= i.value();

        QString oParentId=  v.toObject()["parents"].toArray()[0].toObject()["id"].toString();
        QString oMimeType=  v.toObject()["mimeType"].toString();
        bool    oTrashed=   v.toObject()["labels"].toArray()[0].toObject()["trashed"].toBool();

        if(oParentId==parentId){

            if(files && !foldersAndFiles){

                if(oMimeType != "application/vnd.google-apps.folder"){

                    if(!noTrash){
                        out.insert(v.toObject()["id"].toString(),v);
                    }
                    else{
                        if(!oTrashed){
                            out.insert(v.toObject()["id"].toString(),v);
                        }
                    }
                }

            }

            if(folders && !foldersAndFiles){

                if(oMimeType == "application/vnd.google-apps.folder"){

                    if(!noTrash){
                        out.insert(v.toObject()["id"].toString(),v);
                    }
                    else{
                        if(!oTrashed){
                            out.insert(v.toObject()["id"].toString(),v);
                        }
                    }
                }
            }

            if(foldersAndFiles){

                    if(!noTrash){
                        out.insert(v.toObject()["id"].toString(),v);
                    }
                    else{
                        if(!oTrashed){
                            out.insert(v.toObject()["id"].toString(),v);
                        }
                    }

            }

        }

       i++;

    }


    return out;

}

//=======================================================================================

bool MSGoogleDrive::isFile(QJsonValue remoteObject){
    if(remoteObject.toObject()["mimeType"].toString()!="application/vnd.google-apps.folder"){
        return true;
    }
    return false;
}

//=======================================================================================

bool MSGoogleDrive::isFolder(QJsonValue remoteObject){
    if(remoteObject.toObject()["mimeType"].toString()=="application/vnd.google-apps.folder"){
        return true;
    }
    return false;
}

//=======================================================================================

void MSGoogleDrive::readRemote(QString parentId,QString currentPath){



    QHash<QString,QJsonValue> list=this->get(parentId,MSCloudProvider::CloudObjects::FilesAndFolders | MSCloudProvider::CloudObjects::NoTrash);

    QHash<QString,QJsonValue>::iterator i=list.begin();

    while(i!=list.end()){

        QJsonObject o=i.value().toObject();

        if(o["labels"].toObject()["trashed"].toBool()){
            i++;
            continue; // skip trashed objects
        }

        if( this->filterGoogleDocsMimeTypes(o["mimeType"].toString()) ){
            if(!this->getFlag("convertDoc")){
                i++;
                continue; // skip google docs objects
            }
        }

        MSFSObject fsObject;
        fsObject.path=currentPath;

        fsObject.remote.md5Hash=o["md5Checksum"].toString();

        fsObject.remote.fileSize=  o["fileSize"].toString().toInt();
        fsObject.remote.data=o;
        fsObject.remote.exist=true;

        fsObject.state=MSFSObject::ObjectState::NewRemote;

        if(this->getFlag("convertDoc") && this->filterGoogleDocsMimeTypes(o["mimeType"].toString())){

            fsObject.isDocFormat=true;


//            // get real data for remote doc object
//            MSRequest *req = new MSRequest();


//            req->setMethod("get");

//            req->addHeader("Authorization","Bearer "+this->access_token);

//            if(fsObject.remote.data["mimeType"].toString()=="application/vnd.google-apps.spreadsheet"){

//                QString url=fsObject.remote.data["exportLinks"].toObject()["application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"].toString();
//                req->setRequestUrl(fsObject.remote.data["exportLinks"].toObject()["application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"].toString());

//                req->addQueryItem("id",fsObject.remote.data["id"].toString());
//                req->addQueryItem("exportFormat","xlsx");
//            }

//            if(fsObject.remote.data["mimeType"].toString()=="application/vnd.google-apps.document"){

//                req->setRequestUrl(fsObject.remote.data["exportLinks"].toObject()["application/vnd.openxmlformats-officedocument.wordprocessingml.document"].toString());

//                req->addQueryItem("id",fsObject.remote.data["id"].toString());
//                req->addQueryItem("exportFormat","docx");
//            }

//            if(fsObject.remote.data["mimeType"].toString()=="application/vnd.google-apps.presentation"){

//                req->setRequestUrl(fsObject.remote.data["exportLinks"].toObject()["application/vnd.openxmlformats-officedocument.presentationml.presentation"].toString());

//                req->addQueryItem("id",fsObject.remote.data["id"].toString());
//                req->addQueryItem("exportFormat","pptx");
//            }

//            req->exec();

//            QString filePath=this->workPath+fsObject.path+"ro.tmp";

//            QFile file(filePath);
//            file.open(QIODevice::WriteOnly );
//            QDataStream outk(&file);

//            QByteArray ba;
//            ba.append(req->readReplyText());

//            int sz=ba.size();
//            fsObject.remote.fileSize=sz;


//            outk.writeRawData(ba.data(),ba.size()) ;

//            file.close();

//            fsObject.remote.md5Hash=this->fileChecksum(filePath,QCryptographicHash::Md5);
//            file.remove();

//            delete(req);
        }


        if(this->isFile(o)){

            fsObject.fileName=o["originalFilename"].toString();
            if(fsObject.fileName==""){
                fsObject.fileName=o["title"].toString();
            }
            fsObject.remote.objectType=MSRemoteFSObject::Type::file;
            fsObject.remote.modifiedDate=this->toMilliseconds(o["modifiedDate"].toString(),true);
        }

        if(this->isFolder(o)){

            fsObject.fileName=o["title"].toString();
            if(fsObject.fileName==""){
                fsObject.fileName=o["originalFilename"].toString();
            }
            fsObject.remote.objectType=MSRemoteFSObject::Type::folder;
            fsObject.remote.modifiedDate=this->toMilliseconds(o["modifiedDate"].toString(),true);

        }


        if(! this->filterServiceFileNames(currentPath+fsObject.fileName)){// skip service files and dirs
            i++;
            continue;
        }


        if(this->isFolder(o)){// recursive self calling
            this->readRemote(o["id"].toString(),currentPath+fsObject.fileName+"/");
        }

        if(this->getFlag("useInclude")){//  --use-include

            if( this->filterIncludeFileNames(currentPath+fsObject.fileName)){
                i++;
                continue;
            }
        }
        else{// use exclude by default

            if(! this->filterExcludeFileNames(currentPath+fsObject.fileName)){
                i++;
                continue;
            }
        }

        this->syncFileList.insert(currentPath+fsObject.fileName, fsObject);

//        if(this->isFolder(o)){// recursive self calling
//            this->readRemote(o["id"].toString(),currentPath+fsObject.fileName+"/");
//        }


        i++;
    }
}



//=======================================================================================

void MSGoogleDrive::readLocal(QString path){



    QDir dir(path);
    QDir::Filters entryInfoList_flags=QDir::Files|QDir::Dirs |QDir::NoDotAndDotDot;

    if(! this->getFlag("noHidden")){// if show hidden
        entryInfoList_flags= entryInfoList_flags | QDir::System | QDir::Hidden;
    }

        QFileInfoList files = dir.entryInfoList(entryInfoList_flags);

        foreach(const QFileInfo &fi, files){

            QString Path = fi.absoluteFilePath();
            QString relPath=fi.absoluteFilePath().replace(this->workPath,"");

            if(! this->filterServiceFileNames(relPath)){// skip service files and dirs
                continue;
            }


            if(fi.isDir()){

                readLocal(Path);
            }

            if(this->getFlag("useInclude")){//  --use-include

                if( this->filterIncludeFileNames(relPath)){
                    continue;
                }
            }
            else{// use exclude by default

                if(! this->filterExcludeFileNames(relPath)){
                    continue;
                }
            }



            QHash<QString,MSFSObject>::iterator i=this->syncFileList.find(relPath);



            if(i!=this->syncFileList.end()){// if object exists in Google Drive

                MSFSObject* fsObject = &(i.value());


                fsObject->local.fileSize=  fi.size();
                fsObject->local.md5Hash= this->fileChecksum(Path,QCryptographicHash::Md5);
                fsObject->local.exist=true;
                fsObject->getLocalMimeType(this->workPath);

                if(fi.isDir()){
                    fsObject->local.objectType=MSLocalFSObject::Type::folder;
                    fsObject->local.modifiedDate=this->toMilliseconds(fi.lastModified(),true);
                }
                else{

                    fsObject->local.objectType=MSLocalFSObject::Type::file;
                    fsObject->local.modifiedDate=this->toMilliseconds(fi.lastModified(),true);

                }

                if(this->getFlag("convertDoc") && this->filterOfficeMimeTypes(fsObject->local.mimeType )){

                    fsObject->isDocFormat=true;
                }

                fsObject->state=this->filelist_defineObjectState(fsObject->local,fsObject->remote);

            }
            else{

                MSFSObject fsObject;

                fsObject.state=MSFSObject::ObjectState::NewLocal;

                if(relPath.lastIndexOf("/")==0){
                    fsObject.path="/";
                }
                else{
                    fsObject.path=QString(relPath).left(relPath.lastIndexOf("/"))+"/";
                }

                fsObject.fileName=fi.fileName();
                fsObject.getLocalMimeType(this->workPath);

                fsObject.local.fileSize=  fi.size();
                fsObject.local.md5Hash= this->fileChecksum(Path,QCryptographicHash::Md5);
                fsObject.local.exist=true;

                if(fi.isDir()){
                    fsObject.local.objectType=MSLocalFSObject::Type::folder;
                    fsObject.local.modifiedDate=this->toMilliseconds(fi.lastModified(),true);
                }
                else{

                    fsObject.local.objectType=MSLocalFSObject::Type::file;
                    fsObject.local.modifiedDate=this->toMilliseconds(fi.lastModified(),true);

                }

                fsObject.state=this->filelist_defineObjectState(fsObject.local,fsObject.remote);

                if(this->getFlag("convertDoc") && this->filterOfficeMimeTypes(fsObject.local.mimeType )){

                    fsObject.isDocFormat=true;
                }

                this->syncFileList.insert(relPath,fsObject);

            }


//            if(fi.isDir()){

//                readLocal(Path);
//            }


        }

}



//=======================================================================================

bool MSGoogleDrive::filterServiceFileNames(QString path){// return false if input path is service filename

    QString reg=this->trashFileName+"*|"+this->tokenFileName+"|"+this->stateFileName+"|.include|.exclude|~";
    QRegExp regex(reg);
    int ind = regex.indexIn(path);

    if(ind != -1){
        return false;
    }
    return true;

}

//=======================================================================================

bool MSGoogleDrive::filterIncludeFileNames(QString path){// return false if input path contain one of include mask

    if(this->includeList==""){
        return true;
    }

    // catch paths with  beginning masks from include/exclude lists
    bool isBegin=false;
    bool start;
    QRegularExpression regex3(path);
    regex3.patternErrorOffset();
    QRegularExpressionMatch m1= regex3.match(this->includeList);

    if(m1.hasMatch()){
        start=m1.capturedStart();

        if((this->includeList.mid(start-1,1)=="|") ||(start==0)){
            isBegin=true;
        }
    }

    QRegularExpression regex2(this->includeList);

    regex2.patternErrorOffset();

    QRegularExpressionMatch m = regex2.match(path);

    if(m.hasMatch()){
        return false;
    }
    else{
        if(isBegin){
            return false;
        }
        else{
            return true;
        }
    }

}

//=======================================================================================

bool MSGoogleDrive::filterExcludeFileNames(QString path){// return false if input path contain one of exclude mask

    if(this->excludeList==""){
        return true;
    }

    // catch paths with  beginning masks from include/exclude lists
    bool isBegin=false;
    bool start;
    QRegularExpression regex3(path);
    regex3.patternErrorOffset();
    QRegularExpressionMatch m1= regex3.match(this->excludeList);

    if(m1.hasMatch()){
        start=m1.capturedStart();

        if((this->excludeList.mid(start-1,1)=="|") ||(start==0)){
            isBegin=true;
        }
    }


    QRegularExpression regex2(this->excludeList);

    regex2.patternErrorOffset();

    QRegularExpressionMatch m = regex2.match(path);

    if(m.hasMatch()){
        return false;
    }
    else{
        if(isBegin){
            return false;
        }
        else{
            return true;
        }
    }


}

//=======================================================================================

bool MSGoogleDrive::filterGoogleDocsMimeTypes(QString mime){// return true if this mime is Google document

    if((mime == "application/vnd.google-apps.document")||
       (mime == "application/vnd.google-apps.presentation")||
       (mime == "application/vnd.google-apps.spreadsheet")){

        return true;
    }
    else{
        return false;
    }
}

//=======================================================================================

bool MSGoogleDrive::filterOfficeMimeTypes(QString mime){// return true if this mime is Office document

    QRegularExpression regex2("word|excel|powerpoint|ppt|xls|doc|opendocument");

    regex2.patternErrorOffset();

    QRegularExpressionMatch m = regex2.match(mime);

    if(m.hasMatch()){
        return true;
    }
    else{
        return false;
    }

}

//=======================================================================================

QHash<QString,MSFSObject> MSGoogleDrive::getRemoteFileList(){



    this->createHashFromRemote();
    this->readRemote(this->getRoot(),"/");// top level files and folders

    return this->syncFileList;
}

//=======================================================================================

void MSGoogleDrive::createSyncFileList(){

    if(this->getFlag("useInclude")){
        QFile key(this->workPath+"/.include");

        if(key.open(QIODevice::ReadOnly)){

            QTextStream instream(&key);
            QString line;
            while(!instream.atEnd()){

                QString line=instream.readLine();
                if(line.isEmpty()){
                        continue;
                }

                this->includeList=this->includeList+line+"|";
            }
            this->includeList=this->includeList.left(this->includeList.size()-1);

            QRegularExpression regex2(this->includeList);

            if(regex2.patternErrorOffset()!=-1){
                qStdOut()<<"Include filelist contains errors. Program will be terminated.";
                exit(0);
            }

        }
    }
    else{
        QFile key(this->workPath+"/.exclude");

        if(key.open(QIODevice::ReadOnly)){

            QTextStream instream(&key);
            QString line;
            while(!instream.atEnd()){

                QString line=instream.readLine();
                if(line.isEmpty()){
                        continue;
                }
                this->excludeList=this->excludeList+line+"|";
            }
            this->excludeList=this->excludeList.left(this->excludeList.size()-1);

            QRegularExpression regex2(this->excludeList);

            if(regex2.patternErrorOffset()!=-1){
                qStdOut()<<"Exclude filelist contains errors. Program will be terminated.";
                exit(0);
            }
        }
    }



    this->filterIncludeFileNames(QString("/f1/f1-2"));
    this->filterIncludeFileNames(QString("/f1/f1-2/qq"));
    this->filterIncludeFileNames(QString("/f1/qee"));
    this->filterIncludeFileNames(QString("/f1-2"));






    qStdOut()<< "Reading remote files"<<endl;


    this->createHashFromRemote();

    // begin create



    this->readRemote(this->getRoot(),"/");// top level files and folders

    qStdOut()<<"Reading local files and folders" <<endl;

    this->readLocal(this->workPath);


    this->doSync();

}

//=======================================================================================

MSFSObject::ObjectState MSGoogleDrive::filelist_defineObjectState(MSLocalFSObject local, MSRemoteFSObject remote){



        if((local.exist)&&(remote.exist)){ //exists both files

            if(local.md5Hash==remote.md5Hash){


                    return MSFSObject::ObjectState::Sync;

            }
            else{

                // compare last modified date for local and remote
                if(local.modifiedDate==remote.modifiedDate){

                    if(this->strategy==MSCloudProvider::SyncStrategy::PreferLocal){
                        return MSFSObject::ObjectState::ChangedLocal;
                    }
                    else{
                        return MSFSObject::ObjectState::ChangedRemote;
                    }

                }
                else{

                    if(local.modifiedDate > remote.modifiedDate){
                        return MSFSObject::ObjectState::ChangedLocal;
                    }
                    else{
                        return MSFSObject::ObjectState::ChangedRemote;
                    }

                }
            }


        }


        if((local.exist)&&(!remote.exist)){ //exist only local file

            if(this->strategy == MSCloudProvider::SyncStrategy::PreferLocal){
                return  MSFSObject::ObjectState::NewLocal;
            }
            else{
                return  MSFSObject::ObjectState::DeleteRemote;
            }
        }


        if((!local.exist)&&(remote.exist)){ //exist only remote file

            if(this->strategy == MSCloudProvider::SyncStrategy::PreferLocal){
                return  MSFSObject::ObjectState::DeleteLocal;
            }
            else{
                return  MSFSObject::ObjectState::NewRemote;
            }
        }





}

//=======================================================================================

QHash<QString,MSFSObject> MSGoogleDrive::filelist_getFSObjectsByState(MSFSObject::ObjectState state){

    QHash<QString,MSFSObject> out;

    QHash<QString,MSFSObject>::iterator i=this->syncFileList.begin();

    while(i != this->syncFileList.end()){

        if(i.value().state == state){
            out.insert(i.key(),i.value());
        }

        i++;
    }

    return out;
}

//=======================================================================================

QHash<QString,MSFSObject> MSGoogleDrive::filelist_getFSObjectsByState(QHash<QString,MSFSObject> fsObjectList,MSFSObject::ObjectState state){

    QHash<QString,MSFSObject> out;

    QHash<QString,MSFSObject>::iterator i=fsObjectList.begin();

    while(i != fsObjectList.end()){

        if(i.value().state == state){
            out.insert(i.key(),i.value());
        }

        i++;
    }

    return out;
}

//=======================================================================================

QHash<QString,MSFSObject> MSGoogleDrive::filelist_getFSObjectsByTypeLocal(MSLocalFSObject::Type type){

    QHash<QString,MSFSObject> out;

    QHash<QString,MSFSObject>::iterator i=this->syncFileList.begin();

    while(i != this->syncFileList.end()){

        if(i.value().local.objectType == type){
            out.insert(i.key(),i.value());
        }

        i++;
    }

    return out;

}

//=======================================================================================

QHash<QString,MSFSObject> MSGoogleDrive::filelist_getFSObjectsByTypeRemote(MSRemoteFSObject::Type type){

    QHash<QString,MSFSObject> out;

    QHash<QString,MSFSObject>::iterator i=this->syncFileList.begin();

    while(i != this->syncFileList.end()){

        if(i.value().remote.objectType == type){
            out.insert(i.key(),i.value());
        }

        i++;
    }

    return out;

}

//===================================================================================

bool MSGoogleDrive::filelist_FSObjectHasParent(MSFSObject fsObject){

    if(fsObject.path=="/"){
        return false;
    }
    else{
        return true;
    }

    if(fsObject.path.count("/")>=1){
        return true;
    }
    else{
        return false;
    }
}

//=======================================================================================

MSFSObject MSGoogleDrive::filelist_getParentFSObject(MSFSObject fsObject){

    QString parentPath;

    if((fsObject.local.objectType==MSLocalFSObject::Type::file) || (fsObject.remote.objectType==MSRemoteFSObject::Type::file)){
        parentPath=fsObject.path.left(fsObject.path.lastIndexOf("/"));
    }
    else{
        parentPath=fsObject.path.left(fsObject.path.lastIndexOf("/"));
    }

    if(parentPath==""){
        parentPath="/";
    }

    QHash<QString,MSFSObject>::iterator parent=this->syncFileList.find(parentPath);

    if(parent != this->syncFileList.end()){
        return parent.value();
    }
    else{
        return MSFSObject();
    }

}

//=======================================================================================


void MSGoogleDrive::filelist_populateChanges(MSFSObject changedFSObject){

    QHash<QString,MSFSObject>::iterator object=this->syncFileList.find(changedFSObject.path+changedFSObject.fileName);

    if(object != this->syncFileList.end()){
        object.value().local=changedFSObject.local;
        object.value().remote.data=changedFSObject.remote.data;
    }

}

//=======================================================================================

void MSGoogleDrive::doSync(){

    QHash<QString,MSFSObject>::iterator lf;

    if(this->strategy == MSCloudProvider::SyncStrategy::PreferLocal){

        // create new folder structure on remote

        qStdOut()<<"Checking folder structure on remote" <<endl;

        QHash<QString,MSFSObject> localFolders=this->filelist_getFSObjectsByTypeLocal(MSLocalFSObject::Type::folder);
        localFolders=this->filelist_getFSObjectsByState(localFolders,MSFSObject::ObjectState::NewLocal);

        lf=localFolders.begin();

        while(lf != localFolders.end()){

            this->remote_createDirectory(lf.key());

            lf++;
        }
    }
    else{

        // create new folder structure on local

        qStdOut()<<"Checking folder structure on local" <<endl;

        QHash<QString,MSFSObject> remoteFolders=this->filelist_getFSObjectsByTypeRemote(MSRemoteFSObject::Type::folder);
        remoteFolders=this->filelist_getFSObjectsByState(remoteFolders,MSFSObject::ObjectState::NewRemote);

        lf=remoteFolders.begin();

        while(lf != remoteFolders.end()){

            this->local_createDirectory(this->workPath+lf.key());

            lf++;
        }

        // trash local folder
        QHash<QString,MSFSObject> trashFolders=this->filelist_getFSObjectsByTypeLocal(MSLocalFSObject::Type::folder);
        trashFolders=this->filelist_getFSObjectsByState(trashFolders,MSFSObject::ObjectState::DeleteRemote);

        lf=trashFolders.begin();

        while(lf != trashFolders.end()){


            this->local_removeFolder(lf.key());

            lf++;
        }

    }




    // FORCING UPLOAD OR DOWNLOAD FILES AND FOLDERS
    if(this->getFlag("force")){

        if(this->getOption("force")=="download"){

            qStdOut()<<"Start downloading in force mode" <<endl;

            lf=this->syncFileList.begin();

            for(;lf != this->syncFileList.end();lf++){

                MSFSObject obj=lf.value();

                if((obj.state == MSFSObject::ObjectState::Sync)||
                   (obj.state == MSFSObject::ObjectState::NewRemote)||
                   (obj.state == MSFSObject::ObjectState::DeleteLocal)||
                   (obj.state == MSFSObject::ObjectState::ChangedLocal)||
                   (obj.state == MSFSObject::ObjectState::ChangedRemote) ){

                    qStdOut()<< obj.path<<obj.fileName <<" Forced downloading." <<endl;

                    this->remote_file_get(&obj);
                }

            }

        }
        else{
            if(this->getOption("force")=="upload"){

                qStdOut()<<"Start uploading in force mode" <<endl;

                lf=this->syncFileList.begin();

                for(;lf != this->syncFileList.end();lf++){

                    MSFSObject obj=lf.value();

                    if((obj.state == MSFSObject::ObjectState::Sync)||
                       (obj.state == MSFSObject::ObjectState::NewLocal)||
                       (obj.state == MSFSObject::ObjectState::DeleteRemote)||
                       (obj.state == MSFSObject::ObjectState::ChangedLocal)||
                       (obj.state == MSFSObject::ObjectState::ChangedRemote) ){

                        qStdOut()<< obj.path<<obj.fileName <<" Forced uploading." <<endl;

                        if(obj.remote.exist){

                            this->remote_file_update(&obj);
                        }
                        else{

                            this->remote_file_insert(&obj);
                        }


                    }

                }
            }
            else{
                // error
            }
        }


        if(this->getFlag("dryRun")){
            return;
        }

        // save state file

        QJsonDocument state;
        QJsonObject jso;
        jso.insert("change_stamp",QString("0"));

        QJsonObject jts;
        jts.insert("nsec",QString("0"));
        jts.insert("sec",QString::number(QDateTime( QDateTime::currentDateTime()).toMSecsSinceEpoch()));

        jso.insert("last_sync",jts);
        state.setObject(jso);

        QFile key(this->workPath+"/"+this->stateFileName);
        key.open(QIODevice::WriteOnly | QIODevice::Text);
        QTextStream outk(&key);
        outk << state.toJson();
        key.close();


            qStdOut()<<"Syncronization end" <<endl;

            return;
    }



    // SYNC FILES AND FOLDERS

    qStdOut()<<"Start syncronization" <<endl;

    lf=this->syncFileList.begin();

    for(;lf != this->syncFileList.end();lf++){

        MSFSObject obj=lf.value();

        if((obj.state == MSFSObject::ObjectState::Sync)){

            continue;
        }

        switch(obj.state){

            case MSFSObject::ObjectState::ChangedLocal:

                qStdOut()<< obj.path<<obj.fileName <<" Changed local. Uploading." <<endl;

                this->remote_file_update(&obj);

                break;

            case MSFSObject::ObjectState::NewLocal:

                if((obj.local.modifiedDate > this->lastSyncTime)&&(this->lastSyncTime != 0)){// object was added after last sync

                    qStdOut()<< obj.path<<obj.fileName <<" New local. Uploading." <<endl;

                    this->remote_file_insert(&obj);

                }
                else{

                    if(this->strategy == MSCloudProvider::SyncStrategy::PreferLocal){

                        qStdOut()<< obj.path<<obj.fileName <<" New local. Uploading." <<endl;

                        this->remote_file_insert(&obj);

                    }
                    else{

                        qStdOut()<< obj.path<<obj.fileName <<" Delete remote. Delete local." <<endl;

                        if((obj.local.objectType == MSLocalFSObject::Type::file)||(obj.remote.objectType == MSRemoteFSObject::Type::file)){
                            this->local_removeFile(obj.path+obj.fileName);
                        }
                        else{
                            this->local_removeFolder(obj.path+obj.fileName);
                        }


                    }
                }


                break;

            case MSFSObject::ObjectState::ChangedRemote:

                qStdOut()<< obj.path<<obj.fileName <<" Changed remote. Downloading." <<endl;

                this->remote_file_get(&obj);

                break;


            case MSFSObject::ObjectState::NewRemote:

                if((obj.remote.modifiedDate > this->lastSyncTime)&&(this->lastSyncTime != 0)){// object was added after last sync

                    if(this->strategy == MSCloudProvider::SyncStrategy::PreferLocal){

                        qStdOut()<< obj.path<<obj.fileName <<" Delete local. Deleting remote." <<endl;

                        this->remote_file_trash(&obj);

                    }
                    else{
                        qStdOut()<< obj.path<<obj.fileName <<" New remote. Downloading." <<endl;

                        this->remote_file_get(&obj);
                    }


                }
                else{

                    if(this->strategy == MSCloudProvider::SyncStrategy::PreferLocal){

                        qStdOut()<< obj.path<<obj.fileName <<" Delete local. Deleting remote." <<endl;

                        this->remote_file_trash(&obj);
                    }
                    else{

                        qStdOut()<< obj.path<<obj.fileName <<" New remote. Downloading." <<endl;

                        this->remote_file_get(&obj);
                    }
                }

                break;


            case MSFSObject::ObjectState::DeleteLocal:

                if((obj.remote.modifiedDate > this->lastSyncTime)&&(this->lastSyncTime != 0)){// object was added after last sync

                    qStdOut()<< obj.path<<obj.fileName <<" New remote. Downloading." <<endl;

                    this->remote_file_get(&obj);

                    break;
                }

                qStdOut()<< obj.fileName <<" Delete local. Deleting remote." <<endl;

                this->remote_file_trash(&obj);

                break;

            case MSFSObject::ObjectState::DeleteRemote:

                if((obj.local.modifiedDate > this->lastSyncTime)&&(this->lastSyncTime != 0)){// object was added after last sync

                    if(this->strategy == MSCloudProvider::SyncStrategy::PreferLocal){

                        qStdOut()<< obj.path<<obj.fileName <<" New local. Uploading." <<endl;

                        this->remote_file_insert(&obj);
                    }
                    else{
                        qStdOut()<< obj.path<<obj.fileName <<" Delete remote. Deleting local." <<endl;

                        if((obj.local.objectType == MSLocalFSObject::Type::file)||(obj.remote.objectType == MSRemoteFSObject::Type::file)){
                            this->local_removeFile(obj.path+obj.fileName);
                        }
                        else{
                            this->local_removeFolder(obj.path+obj.fileName);
                        }

                    }
                }
                else{

                    if(this->strategy == MSCloudProvider::SyncStrategy::PreferLocal){

                        qStdOut()<< obj.path<<obj.fileName <<" New local. Uploading." <<endl;

                        this->remote_file_insert(&obj);

                    }
                    else{

                        qStdOut()<< obj.path<<obj.fileName <<" Delete remote. Deleting local." <<endl;

                        if((obj.local.objectType == MSLocalFSObject::Type::file)||(obj.remote.objectType == MSRemoteFSObject::Type::file)){
                            this->local_removeFile(obj.path+obj.fileName);
                        }
                        else{
                            this->local_removeFolder(obj.path+obj.fileName);
                        }


                    }
                }


                break;


        }


    }

    if(this->getFlag("dryRun")){
        return;
    }

    // save state file

    QJsonDocument state;
    QJsonObject jso;
    jso.insert("change_stamp",QString("0"));

    QJsonObject jts;
    jts.insert("nsec",QString("0"));
    jts.insert("sec",QString::number(QDateTime( QDateTime::currentDateTime()).toMSecsSinceEpoch()));

    jso.insert("last_sync",jts);
    state.setObject(jso);

    QFile key(this->workPath+"/"+this->stateFileName);
    key.open(QIODevice::WriteOnly | QIODevice::Text);
    QTextStream outk(&key);
    outk << state.toJson();
    key.close();


        qStdOut()<<"Syncronization end" <<endl;
}


// ============= REMOTE FUNCTIONS BLOCK =============
//=======================================================================================

bool MSGoogleDrive::remote_file_generateIDs(int count){

    QList<QString> lst;

    while(count > 0){
        MSRequest *req = new MSRequest();

        req->setRequestUrl("https://www.googleapis.com/drive/v2/files/generateIds");
        req->setMethod("get");

        int c=0;

        if(count<1000){
            c=count;
            count =0;
        }
        else{
            c=1000;
            count-=1000;
        }

        req->addQueryItem("maxResults",          QString::number(c));
        req->addQueryItem("space",               "drive");
        req->addQueryItem("access_token",           this->access_token);

        req->exec();


        if(!req->replyOK()){
            req->printReplyError();
            exit(1);
        }

        if(!this->testReplyBodyForError(req->readReplyText())){
            qStdOut()<< "Service error. " << this->getReplyErrorString(req->readReplyText()) << endl;
            exit(0);
        }


        QString content= req->readReplyText();

        QJsonDocument json = QJsonDocument::fromJson(content.toUtf8());
        QJsonObject job = json.object();
        QJsonArray v=job["ids"].toArray();

        for(int i=0;i<v.size();i++){
            lst.append(v[i].toString());
        }

        delete(req);

    }

    this->ids_list.setList(lst);

    if(lst.size()>0){
       return true;
    }
    else{
        return false;
    }

}

//=======================================================================================
// download file from cloud
void MSGoogleDrive::remote_file_get(MSFSObject* object){

    if(this->getFlag("dryRun")){
        return;
    }

    QString id=object->remote.data["id"].toString();

    MSRequest *req = new MSRequest();

    req->setRequestUrl("https://www.googleapis.com/drive/v2/files/"+id);
    req->setMethod("get");

    req->addHeader("Authorization","Bearer "+this->access_token);

    req->addQueryItem("alt",                    "media");

    // document conversion support
    if(object->isDocFormat && this->getFlag("convertDoc")){

        QString ftype= object->fileName.right(object->fileName.size()- object->fileName.lastIndexOf(".")-1)  ;

        if(object->remote.data["mimeType"].toString()=="application/vnd.google-apps.spreadsheet"){
            object->local.mimeType="application/vnd.openxmlformats-officedocument.spreadsheetml.sheet";

            if(object->local.exist== false){
                object->fileName=object->fileName+".xlsx";
            }


            req->setRequestUrl(object->remote.data["exportLinks"].toObject()[object->local.mimeType].toString());

            req->addQueryItem("id",id);

            if(ftype=="ods"){
                req->addQueryItem("exportFormat","ods");
            }
            else{
                req->addQueryItem("exportFormat","xlsx");
            }

        }

        if(object->remote.data["mimeType"].toString()=="application/vnd.google-apps.document"){
            object->local.mimeType="application/vnd.openxmlformats-officedocument.wordprocessingml.document";

            if(object->local.exist== false){
                object->fileName=object->fileName+".docx";
            }

            req->setRequestUrl(object->remote.data["exportLinks"].toObject()[object->local.mimeType].toString());

            req->addQueryItem("id",id);

            if(ftype=="odt"){
                req->addQueryItem("exportFormat","odt");
            }
            else{
                req->addQueryItem("exportFormat","docx");
            }
        }

        if(object->remote.data["mimeType"].toString()=="application/vnd.google-apps.presentation"){
            object->local.mimeType="application/vnd.openxmlformats-officedocument.presentationml.presentation";

            if(object->local.exist== false){
                object->fileName=object->fileName+".pptx";
            }

            req->setRequestUrl(object->remote.data["exportLinks"].toObject()[object->local.mimeType].toString());

            req->addQueryItem("id",id);
            req->addQueryItem("exportFormat","pptx");
//            if(ftype=="odp"){
//                req->addQueryItem("exportFormat","odp");
//            }
//            else{
//                req->addQueryItem("exportFormat","pptx");
//            }

        }

    }


    req->exec();

    QString filePath=this->workPath+object->path+object->fileName;


    if(this->testReplyBodyForError(req->readReplyText())){

        if(object->remote.objectType==MSLocalFSObject::Type::file){

            this->local_writeFileContent(filePath,req);
        }
    }
    else{
        qStdOut() << "Service error. "<< this->getReplyErrorString(req->readReplyText());
    }

//    QFile file(filePath);
//    file.open(QIODevice::WriteOnly );
//    QDataStream outk(&file);

//    QByteArray ba;
//    ba.append(req->readReplyText());

//    int sz=ba.size();

//    if(object->remote.objectType==MSLocalFSObject::Type::file){
//        outk.writeRawData(ba.data(),ba.size()) ;
//    }
//    file.close();

    delete(req);

}

//=======================================================================================

void MSGoogleDrive::remote_file_insert(MSFSObject *object){

    if(object->local.objectType==MSLocalFSObject::Type::folder){

        qStdOut()<< object->fileName << " is a folder. Skipped." <<endl;
        return;
    }

    if(this->getFlag("dryRun")){
        return;
    }

    QString bound="ccross-data";
    MSFSObject po=this->filelist_getParentFSObject(*object);
    QString parentID="";
    if(po.path != ""){
        parentID= po.remote.data["id"].toString();
    }

    MSRequest *req = new MSRequest();

    req->setRequestUrl("https://www.googleapis.com/upload/drive/v2/files");
    req->setMethod("post");

    req->addHeader("Authorization",                     "Bearer "+this->access_token);
    req->addHeader("Content-Type",                      "multipart/related; boundary="+QString(bound).toLocal8Bit());
    req->addQueryItem("uploadType",                     "multipart");

    // document conversion support
    if(object->isDocFormat && this->getFlag("convertDoc")){

        req->addQueryItem("convert",                     "true");
        // correct mime type to ms office mime type

        QString ftype= object->fileName.right(object->fileName.size()- object->fileName.lastIndexOf(".")-1)  ;

        if(ftype=="xlsx"){
            object->local.mimeType="application/vnd.openxmlformats-officedocument.spreadsheetml.sheet";
        }

        if(ftype=="xls"){
            object->local.mimeType="application/vnd.ms-excel";
        }

        if(ftype=="docx"){
            object->local.mimeType="application/vnd.openxmlformats-officedocument.wordprocessingml.document";
        }

        if(ftype=="doc"){
            object->local.mimeType="application/msword";
        }

        if(ftype=="pptx"){
            object->local.mimeType="application/vnd.openxmlformats-officedocument.presentationml.presentation";
        }

        if(ftype=="ppt"){
            object->local.mimeType="application/vnd.ms-powerpoint";
        }

//        if(ftype=="odp"){
//            object->local.mimeType="application/vnd.oasis.opendocument.presentation";
//        }

        if(ftype=="ods"){
            object->local.mimeType="application/vnd.oasis.opendocument.spreadsheet";
        }

        if(ftype=="odt"){
            object->local.mimeType="application/vnd.oasis.opendocument.text";
        }

    }

    // collect request data body

    QByteArray metaData;
    metaData.append(QString("--"+bound+"\r\n").toLocal8Bit());
    metaData.append(QString("Content-Type: application/json; charset=UTF-8\r\n\r\n").toLocal8Bit());

    //make file metadata in json representation
    QJsonObject metaJson;

    if(object->isDocFormat && this->getFlag("convertDoctt")){

        // remove file extension

        metaJson.insert("title",object->fileName.left(object->fileName.lastIndexOf(".")));
    }
    else{
        metaJson.insert("title",object->fileName);
    }

    if(parentID != ""){
        // create parents section
        QJsonArray parents;
        QJsonObject par;
        par.insert("id",parentID);
        parents.append(par);

        metaJson.insert("parents",parents);
    }

    metaData.append(QString(QJsonDocument(metaJson).toJson()).toLocal8Bit());
    metaData.append(QString("\r\n--"+bound+"\r\n").toLocal8Bit());

    QByteArray mediaData;

    mediaData.append(QString("Content-Type: "+object->local.mimeType+"\r\n\r\n").toLocal8Bit());

    QString filePath=this->workPath+object->path+object->fileName;

    // read file content and put him into request body
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)){

        //error file not found
        qStdOut()<<"Unable to open of "+filePath <<endl;
        delete(req);
        return;
    }
    mediaData.append(file.readAll());
    file.close();


    mediaData.append(QString("\r\n--"+bound+"--").toLocal8Bit());

    req->addHeader("Content-Length",QString::number(metaData.length()+mediaData.length()).toLocal8Bit());

    req->post(metaData+mediaData);


    if(!req->replyOK()){
        req->printReplyError();
        exit(1);
    }

    if(!this->testReplyBodyForError(req->readReplyText())){
        qStdOut()<< "Service error. " << this->getReplyErrorString(req->readReplyText()) << endl;
        exit(0);
    }



    QString content=req->readReplyText();

    QJsonDocument json = QJsonDocument::fromJson(content.toUtf8());
    QJsonObject job = json.object();
    object->local.newRemoteID=job["id"].toString();

    if(job["id"].toString()==""){
        qStdOut()<< "Error when upload "+filePath+" on remote" <<endl;
    }

    delete(req);

}


//=======================================================================================

void MSGoogleDrive::remote_file_update(MSFSObject *object){

    if(object->local.objectType==MSLocalFSObject::Type::folder){

        qStdOut()<< object->fileName << " is a folder. Skipped." <<endl;
        return;
    }

    if(this->getFlag("dryRun")){
        return;
    }

    QString bound="ccross-data";

    QString id=object->remote.data["id"].toString();

    MSFSObject po=this->filelist_getParentFSObject(*object);
    QString parentID="";
    if(po.path != ""){
        parentID= po.remote.data["id"].toString();
    }


    MSRequest *req = new MSRequest();

    req->setRequestUrl("https://www.googleapis.com/upload/drive/v2/files/"+id);
    req->setMethod("post");

    req->addHeader("Authorization",                     "Bearer "+this->access_token);
    req->addHeader("Content-Type",                      "multipart/related; boundary="+QString(bound).toLocal8Bit());
    req->addQueryItem("uploadType",                     "multipart");

    if(this->getFlag("noNewRev")){
        req->addQueryItem("newRevision",                     "false");
        req->addQueryItem("pinned",                          "true");
    }


    // collect request data body

    QByteArray metaData;
    metaData.append(QString("--"+bound+"\r\n").toLocal8Bit());
    metaData.append(QString("Content-Type: application/json; charset=UTF-8\r\n\r\n").toLocal8Bit());

    //make file metadata in json representation
    QJsonObject metaJson;

    metaJson.insert("id",object->remote.data["id"].toString());

    if(parentID != ""){
        // create parents section
        QJsonArray parents;
        QJsonObject par;
        par.insert("id",parentID);
        parents.append(par);

        metaJson.insert("parents",parents);
    }

    metaData.append(QString(QJsonDocument(metaJson).toJson()).toLocal8Bit());
    metaData.append(QString("\r\n--"+bound+"\r\n").toLocal8Bit());

    QByteArray mediaData;
    mediaData.append(QString("Content-Type: "+object->local.mimeType+"\r\n\r\n").toLocal8Bit());

    QString filePath=this->workPath+object->path+object->fileName;

    // read file content and put him into request body
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)){

        //error file not found
        qStdOut()<<"Unable to open of "+filePath <<endl;
        delete(req);
        return;
    }
    mediaData.append(file.readAll());
    file.close();


    mediaData.append(QString("\r\n--"+bound+"--").toLocal8Bit());

    req->addHeader("Content-Length",QString::number(metaData.length()+mediaData.length()).toLocal8Bit());

    req->put(metaData+mediaData);


    if(!req->replyOK()){
        req->printReplyError();
        exit(1);
    }

    if(!this->testReplyBodyForError(req->readReplyText())){
        qStdOut()<< "Service error. " << this->getReplyErrorString(req->readReplyText()) << endl;
        exit(0);
    }



    QString content=req->readReplyText();

    QJsonDocument json = QJsonDocument::fromJson(content.toUtf8());
    QJsonObject job = json.object();
    if(job["id"].toString()==""){
        qStdOut()<< "Error when update "+filePath+" on remote" <<endl;
    }

    delete(req);

}

//=======================================================================================

void MSGoogleDrive::remote_file_makeFolder(MSFSObject *object){

    if(this->getFlag("dryRun")){
        return;
    }

    MSRequest *req = new MSRequest();

    req->setRequestUrl("https://www.googleapis.com/drive/v2/files");
    req->setMethod("post");

    req->addHeader("Authorization",                     "Bearer "+this->access_token);
    req->addHeader("Content-Type",                      QString("application/json; charset=UTF-8"));

    QByteArray metaData;

    //make file metadata in json representation
    QJsonObject metaJson;
    metaJson.insert("title",object->fileName);
    metaJson.insert("mimeType",QString("application/vnd.google-apps.folder"));

    metaData.append(QString(QJsonDocument(metaJson).toJson()).toLocal8Bit());

    req->post(metaData);


    if(!req->replyOK()){
        req->printReplyError();
        exit(1);
    }

    if(!this->testReplyBodyForError(req->readReplyText())){
        qStdOut()<< "Service error. " << this->getReplyErrorString(req->readReplyText()) << endl;
        exit(0);
    }



    QString filePath=this->workPath+object->path+object->fileName;

    QString content=req->readReplyText();

    QJsonDocument json = QJsonDocument::fromJson(content.toUtf8());
    QJsonObject job = json.object();
    object->local.newRemoteID=job["id"].toString();
    object->remote.data=job;
    object->remote.exist=true;

    if(job["id"].toString()==""){
        qStdOut()<< "Error when folder create "+filePath+" on remote" <<endl;
    }

    delete(req);

    this->filelist_populateChanges(*object);

}

//=======================================================================================

void MSGoogleDrive::remote_file_makeFolder(MSFSObject *object, QString parentID){

    if(this->getFlag("dryRun")){
        return;
    }

    MSRequest *req = new MSRequest();

    req->setRequestUrl("https://www.googleapis.com/drive/v2/files");
    req->setMethod("post");

    req->addHeader("Authorization",                     "Bearer "+this->access_token);
    req->addHeader("Content-Type",                      QString("application/json; charset=UTF-8"));

    QByteArray metaData;

    //make file metadata in json representation
    QJsonObject metaJson;
    metaJson.insert("title",object->fileName);
    metaJson.insert("mimeType",QString("application/vnd.google-apps.folder"));

    if(parentID != ""){
        // create parents section
        QJsonArray parents;
        QJsonObject par;
        par.insert("id",parentID);
        parents.append(par);

        metaJson.insert("parents",parents);
    }

    metaData.append(QString(QJsonDocument(metaJson).toJson()).toLocal8Bit());

    req->post(metaData);


    if(!req->replyOK()){
        req->printReplyError();
        exit(1);
    }

    if(!this->testReplyBodyForError(req->readReplyText())){
        qStdOut()<< "Service error. " << this->getReplyErrorString(req->readReplyText()) << endl;
        exit(0);
    }



    QString filePath=this->workPath+object->path+object->fileName;

    QString content=req->readReplyText();

    QJsonDocument json = QJsonDocument::fromJson(content.toUtf8());
    QJsonObject job = json.object();
    object->local.newRemoteID=job["id"].toString();
    object->remote.data=job;
    object->remote.exist=true;

    if(job["id"].toString()==""){
        qStdOut()<< "Error when folder create "+filePath+" on remote" <<endl;
    }

    delete(req);

    this->filelist_populateChanges(*object);

}

//=======================================================================================

void MSGoogleDrive::remote_file_trash(MSFSObject *object){

    if(this->getFlag("dryRun")){
        return;
    }

    QString id=object->remote.data["id"].toString();

    MSRequest *req = new MSRequest();

    req->setRequestUrl("https://www.googleapis.com/drive/v2/files/"+id+"/trash");
    req->setMethod("post");

    req->addHeader("Authorization",                     "Bearer "+this->access_token);

    req->exec();


    if(!req->replyOK()){
        req->printReplyError();
        exit(1);
    }

    if(!this->testReplyBodyForError(req->readReplyText())){
        qStdOut()<< "Service error. " << this->getReplyErrorString(req->readReplyText()) << endl;
        exit(0);
    }


    QString content=req->readReplyText();

    QString filePath=this->workPath+object->path+object->fileName;

    QJsonDocument json = QJsonDocument::fromJson(content.toUtf8());
    QJsonObject job = json.object();
    if(job["id"].toString()==""){
        qStdOut()<< "Error when move to trash "+filePath+" on remote" <<endl;
    }

    delete(req);

}

//=======================================================================================

void MSGoogleDrive::remote_createDirectory(QString path){

    if(this->getFlag("dryRun")){
        return;
    }


    QList<QString> dirs=path.split("/");
    QString currPath="";

    for(int i=1;i<dirs.size();i++){

        QHash<QString,MSFSObject>::iterator f=this->syncFileList.find(currPath+"/"+dirs[i]);

        if(f != this->syncFileList.end()){

            currPath=f.key();

            if(f.value().remote.exist){
                continue;
            }

            if(this->filelist_FSObjectHasParent(f.value())){

                MSFSObject parObj=this->filelist_getParentFSObject(f.value());
                this->remote_file_makeFolder(&f.value(),parObj.remote.data["id"].toString());

            }
            else{

                this->remote_file_makeFolder(&f.value());
            }
        }
//        else{// if this path not exists on remote and not listed as local because include/exclude filter working

//            currPath=currPath+"/"+dirs[i];

//            if(this->filelist_FSObjectHasParent(currPath)){

//            }
//            else{
//                this->remote_file_makeFolder();
//            }

//        }
    }
}



// ============= LOCAL FUNCTIONS BLOCK =============
//=======================================================================================

void MSGoogleDrive::local_createDirectory(QString path){

    if(this->getFlag("dryRun")){
        return;
    }

    QDir d;
    d.mkpath(path);

}

//=======================================================================================

void MSGoogleDrive::local_removeFile(QString path){

    if(this->getFlag("dryRun")){
        return;
    }

    QDir trash(this->workPath+"/"+this->trashFileName);

    if(!trash.exists()){
        trash.mkdir(this->workPath+"/"+this->trashFileName);
    }

    QString origPath=this->workPath+path;
    QString trashedPath=this->workPath+"/"+this->trashFileName+path;

    QFile f;
    f.setFileName(origPath);
    bool res=f.rename(trashedPath);

    if((!res)&&(f.exists())){// everwrite trashed file
        QFile ef(trashedPath);
        ef.remove();
        f.rename(trashedPath);
    }

}

//=======================================================================================

void MSGoogleDrive::local_removeFolder(QString path){

    if(this->getFlag("dryRun")){
        return;
    }

    QDir trash(this->workPath+"/"+this->trashFileName);

    if(!trash.exists()){
        trash.mkdir(this->workPath+"/"+this->trashFileName);
    }

    QString origPath=this->workPath+path;
    QString trashedPath=this->workPath+"/"+this->trashFileName+path;

    QDir f;
    f.setPath(origPath);
    bool res=f.rename(origPath,trashedPath);

    if((!res)&&(f.exists())){// everwrite trashed folder
        QDir ef(trashedPath);
        ef.removeRecursively();
        f.rename(origPath,trashedPath);
    }

}


bool MSGoogleDrive::testReplyBodyForError(QString body){// return true if reply is ok

    if(body.contains("\"error\": {")){
        QJsonDocument json = QJsonDocument::fromJson(body.toUtf8());
        QJsonObject job = json.object();

        QJsonValue e=(job["error"].toObject())["code"];
        if(e.isNull()){
            return true;
        }
        else{

            int code=e.toInt(0);
            if(code != 0){
                return false;
            }
            else{
                return true;
            }
        }

    }
    else{
        return true;
    }
}


QString MSGoogleDrive::getReplyErrorString(QString body){

    QJsonDocument json = QJsonDocument::fromJson(body.toUtf8());
    QJsonObject job = json.object();

    QJsonValue e=(job["error"].toObject())["message"];

    return e.toString();

}


