  std::string save_storage_guidance(bool backup_controls) const {
    if(save_storage_preview)return settings_words(
      "저장 위치와 백업 방법은 플레이어에 따라 달라집니다. 내보낸 게임에서 확인하세요.",
      "Save location and backup options depend on the player. Check them in the exported game.",
      "保存先とバックアップ方法はプレイヤーによって異なります。書き出したゲームで確認してください。");
    if(portable_available&&backup_controls)return settings_words(
      "기록은 이 기기에 보관됩니다. 현재 진행 내보내기로 파일에 백업하세요. 자동 동기화되지 않습니다.",
      "Records stay on this device. Use Export current progress for a Files backup; records do not sync automatically.",
      "記録はこの端末に保存されます。「現在の進行を書き出す」でファイルにバックアップできます。自動同期はされません。");
    return settings_words(
      "기록은 이 기기에 보관됩니다. 다른 기기로 자동 동기화되지 않습니다.",
      "Records stay on this device. They do not sync automatically to other devices.",
      "記録はこの端末に保存されます。他の端末には自動同期されません。");
  }
