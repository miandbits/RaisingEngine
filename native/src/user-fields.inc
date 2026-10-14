std::string userFieldPointer(std::string key) {
  std::string out;for(char c:key){if(c=='~')out+="~0";else if(c=='/')out+="~1";else out+=c;}return "/userValues/"+out;
}
int userTextLength(const std::string& text,const std::string& label,const std::string& path) {
  int count=0;for(size_t i=0;i<text.size();++count) {
    const auto first=static_cast<unsigned char>(text[i++]);uint32_t cp=first;int trailing=0;uint32_t minimum=0;
    if(first<0x80){}else if(first>=0xc2&&first<=0xdf){cp=first&0x1f;trailing=1;minimum=0x80;}else if(first>=0xe0&&first<=0xef){cp=first&0x0f;trailing=2;minimum=0x800;}else if(first>=0xf0&&first<=0xf4){cp=first&7;trailing=3;minimum=0x10000;}else need(false,"INVALID_USER_VALUE",label+": 올바른 Unicode 텍스트가 필요합니다.",path);
    need(i+trailing<=text.size(),"INVALID_USER_VALUE",label+": 올바른 Unicode 텍스트가 필요합니다.",path);
    for(int n=0;n<trailing;++n){const auto b=static_cast<unsigned char>(text[i++]);need((b&0xc0)==0x80,"INVALID_USER_VALUE",label+": 올바른 Unicode 텍스트가 필요합니다.",path);cp=(cp<<6)|(b&0x3f);}
    need(cp>=minimum&&cp<=0x10ffff&&!(cp>=0xd800&&cp<=0xdfff),"INVALID_USER_VALUE",label+": 올바른 Unicode 텍스트가 필요합니다.",path);
    need(!(cp<=0x1f||(cp>=0x7f&&cp<=0x9f)||cp==0x2028||cp==0x2029),"INVALID_USER_VALUE",label+": 줄바꿈과 제어 문자는 사용할 수 없습니다.",path);
  }return count;
}
J resolveUserValues(const J& project,const J& options) {
  const auto supplied=options.value("userValues",J::object());need(supplied.is_object(),"INVALID_USER_VALUES","사용자 입력은 객체여야 합니다.","/userValues");
  const auto fields=project.value("userFields",J::array());
  for(auto it=supplied.begin();it!=supplied.end();++it)need(find(fields,it.key()),"UNKNOWN_USER_FIELD","알 수 없는 사용자 필드: "+it.key(),userFieldPointer(it.key()));
  J values=J::object();for(const auto& field:fields){const auto id=str(field,"id"),label=str(field,"label"),path=userFieldPointer(id);const auto value=supplied.contains(id)?supplied.at(id):field.at("initial");need(value.is_string(),"INVALID_USER_VALUE",label+": 텍스트 값을 입력하세요.",path);const auto length=userTextLength(value.get<std::string>(),label,path),min=field.at("minLength").get<int>(),max=field.at("maxLength").get<int>();need(length>=min&&length<=max,"INVALID_USER_VALUE",label+": "+std::to_string(min)+"~"+std::to_string(max)+"자 텍스트가 필요합니다.",path);values[id]=value;}return values;
}
