  std::string reading_history_body() const {
    const auto words=[&](const char* ko,const char* en,const char* ja){return settings_words(ko,en,ja);};
    const auto snapshot=reading_history?reading_history->snapshot():Json{{"entries",Json::array()},{"reason","new-session"},{"omittedEntries",0}};
    std::string out="<div id=\"engine-reading-history-panel\" class=\"reading-item\"><p><b>"+escape(words("최근 읽은 내용 · 이번 실행","Recent reading · this session","最近読んだ内容 · 今回の起動"))+"</b></p>";
    out+="<p>"+escape(words("화면에 표시된 이야기와 고른 답만 담습니다. 저장 파일에 포함되지 않습니다.","Shown story text and chosen responses only. This list is not saved with your game.","表示された物語と選んだ返答だけです。ゲームの保存には含まれません。"))+"</p>";
    std::string context=bind("calendar"),place=bind("location");if(!place.empty())context+=(context.empty()?"":" · ")+place;
    const auto p=phase();const auto phase_label=p=="choice"?words("선택 대기","Waiting for your choice","選択待ち"):p=="dialogue"?words("이야기","Story","物語"):p=="planning"?words("계획","Planning","計画"):p=="ready"?words("활동 준비","Ready","準備完了"):p=="finished"?words("끝맺음","Ending","結末"):words("현재 지점","Current point","現在の地点");
    context+=(context.empty()?"":" · ")+phase_label;out+="<p id=\"engine-reading-history-current\">"+escape(context)+"</p>";
    if(snapshot.value("reason","")=="restored")out+="<p>"+escape(words("불러온 지점부터 새로 기록합니다. 이전에 읽은 내용은 복원하지 않습니다.","Reading starts here after loading. Earlier reading is not restored.","読み込み後のこの地点から記録します。以前の内容は復元されません。"))+"</p>";
    if(snapshot.value("omittedEntries",uint64_t(0))>0)out+="<p>"+escape(words("오래된 내용 일부가 이 짧은 목록에서 제외되었습니다.","Older entries have been removed from this bounded list.","古い内容の一部はこの短い一覧から削除されています。"))+"</p>";
    const auto& entries=snapshot.at("entries");if(entries.empty())out+="<p>"+escape(words("아직 표시된 이야기가 없습니다. 읽으면 여기에 나타납니다.","No story text has been shown yet. Reading will appear here.","まだ物語が表示されていません。読むとここに表示されます。"))+"</p>";
    for(const auto& e:entries){const auto kind=e.value("kind","");std::string label=e.value("date","");if(!e.value("speaker","").empty())label+=(label.empty()?"":" · ")+e.at("speaker").get<std::string>();
      if(kind=="chosen-response")label+=(label.empty()?"":" · ")+words("고른 답","Your response","選んだ返答");
      out+="<div class=\"reading-item\" id=\"engine-reading-history-entry-"+std::to_string(e.at("sequence").get<uint64_t>())+"\"><p><b>"+escape(label)+"</b></p><p>"+escape(e.value("text",""))+"</p>";
      if(e.value("truncated",false))out+="<p>"+escape(words("긴 내용의 일부만 보관했습니다.","Only part of this long entry is retained.","長い内容の一部だけを保持しています。"))+"</p>";out+="</div>";
    }
    return out+"</div>";
  }
