  Json player_preferences=default_player_preferences();
  std::string settings_notice;
  std::string settings_words(const std::string& ko,const std::string& en,const std::string& ja) const {return locale.rfind("en",0)==0?en:locale.rfind("ja",0)==0?ja:ko;}
  std::string settings_body(const Json& e) {
    const auto id=e.value("id","settings");const double w=e.value("width",1160.0),h=e.value("height",500.0),gap=34,cw=(w-gap)/2;
    const auto ink=css_color(e.value("color",std::string("#263C36"))),accent=css_color(e.value("hoverFill",std::string("#345C50"))),line=css_color(e.value("borderColor",std::string("#C7B99A")));
    const auto words=[&](const char* ko,const char* en,const char* ja){return settings_words(ko,en,ja);};
    std::string out;
    auto text=[&](double x,double y,double width,const std::string& value,int size=18,bool subdued=false){out+="<div style=\"position:absolute;left:"+number(x)+"px;top:"+number(y)+"px;width:"+number(width)+"px;height:32px;font-size:"+std::to_string(size)+"px;color:"+(subdued?std::string("#879087"):ink)+";\">"+escape(value)+"</div>";};
    auto button=[&](const std::string& suffix,double x,double y,double width,const std::string& label,const Json& action,bool selected=false,bool disabled=false){const auto key=id+"-"+suffix;actions[key]=action;labels[key]=label;out+="<button class=\"pref-button\" id=\""+escape(key)+"\" style=\"position:absolute;left:"+number(x)+"px;top:"+number(y)+"px;width:"+number(width)+"px;height:42px;padding:6px 4px;box-sizing:border-box;font-size:20px;text-align:center;color:"+(disabled?std::string("#9A9D91"):selected?std::string("#F5EFDF"):ink)+";background-color:"+(disabled?std::string("#EFEADE"):selected?accent:std::string("#E8E2D4"))+";border:1px "+line+";\""+(disabled?" disabled=\"disabled\"":"")+">"+escape((selected?std::string("● "):std::string())+label)+"</button>";};
    auto change=[&](const std::string& key,const Json& value){return Json{{"type","setPlayerSetting"},{"key",key},{"value",value}};};
    auto volume=[&](const std::string& key,double y,const std::string& label){const int value=player_preferences.at(key);text(0,y,cw,label,22);text(cw-94,y,90,std::to_string(value)+" %",22);button(key+"-down",0,y+43,48,"−",change(key,std::max(0,value-10)),false,value==0);button(key+"-up",cw-48,y+43,48,"+",change(key,std::min(100,value+10)),false,value==100);const double rail=cw-122;out+="<div style=\"position:absolute;left:61px;top:"+number(y+59)+"px;width:"+number(rail)+"px;height:10px;background-color:"+line+";\"><div style=\"width:"+number(rail*value/100)+"px;height:10px;background-color:"+accent+";\"></div></div>";};
    text(0,0,cw,words("소리와 화면","Sound & display","音と画面"),25);text(cw+gap,0,cw,words("이야기 읽기","Reading the story","物語を読む"),25);
    volume("musicVolume",61,words("음악 음량","Music volume","音楽の音量"));volume("effectsVolume",177,words("효과음 음량","Sound effects","効果音の音量"));
    text(0,293,cw,words("화면 모드","Display mode","画面モード"),22);
    button("windowed",0,336,(cw-12)/2,words("창 화면","Windowed","ウィンドウ"),change("fullscreen",false),!player_preferences.at("fullscreen").get<bool>());
    button("fullscreen",(cw+12)/2,336,(cw-12)/2,words("전체화면","Fullscreen","全画面"),change("fullscreen",true),player_preferences.at("fullscreen").get<bool>());
    const double right=cw+gap; text(right,61,cw,words("대사 표시 속도","Dialogue speed","台詞の表示速度"),22);
    const std::vector<std::pair<int,std::string>> speeds={{20,words("느리게","Slow","遅い")},{40,words("보통","Normal","普通")},{70,words("빠르게","Fast","速い")},{0,words("즉시","Instant","即時")}};
    for(size_t i=0;i<speeds.size();++i)button("speed-"+std::to_string(speeds[i].first),right+i*(cw+8)/4,104,(cw-24)/4,speeds[i].second,change("textSpeed",speeds[i].first),player_preferences.at("textSpeed")==speeds[i].first);
    text(right,171,cw,words("대사 자동 진행","Automatic dialogue","台詞の自動送り"),22);
    for(bool enabled:{false,true})button(enabled?"auto-on":"auto-off",right+(enabled?(cw+12)/2:0),214,(cw-12)/2,enabled?words("켜기","On","オン"):words("끄기","Off","オフ"),change("autoAdvance",enabled),player_preferences.at("autoAdvance")==enabled);
    const bool autoEnabled=player_preferences.at("autoAdvance");const int delay=player_preferences.at("autoDelay");text(right,280,cw,words("다 읽은 뒤 기다리기 · 1~6초","Wait after text · 1–6 seconds","表示後の待ち時間 · 1～6秒"),20,!autoEnabled);
    button("delay-down",right,320,48,"−",change("autoDelay",std::max(1,delay-1)),false,!player_preferences.at("autoAdvance").get<bool>()||delay==1);text(right+65,329,cw-130,std::to_string(delay)+words("초"," seconds","秒"),22,!autoEnabled);button("delay-up",right+cw-48,320,48,"+",change("autoDelay",std::min(6,delay+1)),false,!player_preferences.at("autoAdvance").get<bool>()||delay==6);
    if(!autoEnabled)text(right,371,cw,words("자동 진행을 켜면 기다리는 시간을 조절할 수 있습니다.","Enable automatic dialogue to adjust the delay.","自動送りをオンにすると待ち時間を調節できます。"),16);
    text(0,406,w,words("선택지에서는 기다립니다. 메뉴를 열면 이야기와 일정이 잠시 멈춥니다.","Choices wait for you. Menus pause dialogue and schedules.","選択肢では待機します。メニュー中は物語と予定が一時停止します。"),18);
    text(0,std::max(451.0,h-35),w,settings_notice.empty()?words("변경한 설정은 바로 적용되며, 다음 실행에도 이어집니다.","Changes apply and save immediately, including on your next launch.","変更はすぐに適用・保存され、次回起動時も引き継がれます。"):settings_notice,18);
    return out;
  }
