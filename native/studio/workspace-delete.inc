  auto remove=new QPushButton(tr("선택 항목 삭제 · 사용처 확인"));remove->setObjectName("workspaceDelete");lb->addWidget(remove);
  connect(remove,&QPushButton::clicked,panel,[this,state,notice]{
    if(bridge.busy()||state->dirty){notice->setText(tr("현재 입력을 먼저 적용하거나 취소하세요."));return;}
    if(state->revision!=view.value("revision").toInt()||state->root!=view.value("root").toString()){notice->setText(tr("프로젝트가 변경되었습니다. 새로 고침하세요."));return;}
    const auto node=state->selected;const auto kind=node.value("kind").toString(),file=node.value("file").toString(),path=node.value("path").toString();
    if(!QStringList{"attribute","flag","activity","action","item","rule","appointment","stateGroup","stateValue","screenElement","presentation","screenRoute"}.contains(kind)){notice->setText(tr("이 항목은 전용 편집기에서 관리하세요."));return;}
    QSet<QString> removed;QMap<QString,QString> names;
    for(const auto& v:state->nodes){const auto n=v.toObject();names[n.value("key").toString()]=n.value("label").toString();if(n.value("file")==file&&(path.isEmpty()||n.value("path")==path||n.value("path").toString().startsWith(path+"/")))removed.insert(n.value("key").toString());}
    QStringList used;
    for(const auto& v:state->edges){const auto e=v.toObject();if(!removed.contains(e.value("to").toString()))continue;if(e.value("file")==file&&(path.isEmpty()||e.value("path")==path||e.value("path").toString().startsWith(path+"/")))continue;used<<names.value(e.value("from").toString(),tr("다른 콘텐츠"));}
    // Also inspect references outside indexed workspace categories (systems,
    // initial-state maps, components). Unknown uses fail closed, never disappear.
    const auto id=node.value("value").toObject().value("id").toString();
    if(!id.isEmpty())for(const auto& value:view.value("documents").toArray()){
      const auto doc=value.toObject();const auto source=doc.value("file").toString();bool found=false;
      std::function<void(QJsonValue,QString,QString)> scan=[&](QJsonValue v,QString at,QString field){if(source==file&&(path.isEmpty()||at==path||at.startsWith(path+"/")))return;if(v.isArray()){int i=0;for(const auto& x:v.toArray())scan(x,at+"/"+QString::number(i++),field);}else if(v.isObject()){const auto object=v.toObject();for(auto it=object.begin();it!=object.end();++it){const auto encoded=QString(it.key()).replace("~","~0").replace("/","~1");if(it.key()==id)found=true;scan(it.value(),at+"/"+encoded,it.key());}}else if(v.isString()&&v.toString()==id&&(field.endsWith("Id")||field.endsWith("Ids")||field=="connections"))found=true;};
      scan(doc.value("data"),{},{});if(found)used<<source;
    }
    used.removeDuplicates();if(!used.isEmpty()){notice->setText(tr("삭제할 수 없습니다. 먼저 다음 사용처의 연결을 바꾸세요: ")+used.mid(0,10).join(", "));return;}
    if(kind=="attribute"){int count=0;for(auto v:state->nodes)if(v.toObject().value("kind")=="attribute")++count;if(count<=1){notice->setText(tr("능력·자원 데이터는 최소 하나가 필요합니다."));return;}}
    if(QMessageBox::question(this,tr("항목 삭제"),tr("‘%1’을 삭제할까요? 되돌리기로 복구할 수 있습니다.").arg(node.value("label").toString()),QMessageBox::Yes|QMessageBox::Cancel,QMessageBox::Cancel)!=QMessageBox::Yes)return;
    if(state->revision!=view.value("revision").toInt()||state->root!=view.value("root").toString()){notice->setText(tr("프로젝트가 변경되어 삭제하지 않았습니다."));return;}
    if(path.isEmpty()){request("edit",{{"edit",QJsonObject{{"type","delete"},{"file",file}}}});return;}
    for(const auto& v:view.value("documents").toArray()){const auto d=v.toObject();if(d.value("file")!=file)continue;auto parts=workspacePath(path);bool ok=false;const int index=parts.takeLast().toInt(&ok);auto parent=logicValueAt(d.value("data"),parts).toArray();if(!ok||index<0||index>=parent.size()){notice->setText(tr("삭제할 위치가 바뀌었습니다. 새로 고침하세요."));return;}parent.removeAt(index);request("edit",{{"edit",QJsonObject{{"type","replace"},{"file",file},{"data",logicReplaceAt(d.value("data"),parts,parent)}}}});return;}
  });
