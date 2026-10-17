#pragma once
#include "json.hpp"

namespace raising {
// Presentation history only: never becomes part of the deterministic run state or save format.
// Retain the last nonzero net change per relationship, including clamping and compound effects.
inline void track_relationship_changes(nlohmann::json& history, const nlohmann::json& before,
                                       const nlohmann::json& after) {
  const auto previous = before.value("relationships", nlohmann::json::object());
  const auto current = after.value("relationships", nlohmann::json::object());
  for (auto person = current.begin(); person != current.end(); ++person) {
    for (auto relation = person.value().begin(); relation != person.value().end(); ++relation) {
      const auto old = previous.value(person.key(), nlohmann::json::object()).value(relation.key(), relation.value());
      const auto delta = relation.value().get<int64_t>() - old.get<int64_t>();
      if (delta != 0) history[person.key()][relation.key()] = delta;
    }
  }
}
}
