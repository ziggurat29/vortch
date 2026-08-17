#pragma once

#include <optional>
#include <string>

namespace vortch {

enum class StatusKind { Quiescent, Processing, Done, Error };

inline std::string toString(StatusKind k) {
  switch (k) {
    case StatusKind::Quiescent:  return "quiescent";
    case StatusKind::Processing: return "processing";
    case StatusKind::Done:       return "done";
    case StatusKind::Error:      return "error";
  }
  return "quiescent";
}

inline std::optional<StatusKind> statusFromString(const std::string& s) {
  if (s == "quiescent")  return StatusKind::Quiescent;
  if (s == "processing") return StatusKind::Processing;
  if (s == "done")       return StatusKind::Done;
  if (s == "error")      return StatusKind::Error;
  return std::nullopt;
}

// What a processor reports for one job.
struct JobReport {
  std::string           jobId;
  StatusKind            kind = StatusKind::Processing;
  std::optional<double> progress;  // 0..1
  std::string           text;
};

// What the UI renders (badge). Aggregated/normalized from job reports.
struct InstanceStatus {
  StatusKind            kind = StatusKind::Quiescent;
  std::optional<double> progress;
  std::string           shortText;
  int                   count = 0;
};

inline bool operator==(const InstanceStatus& a, const InstanceStatus& b) {
  return a.kind == b.kind && a.progress == b.progress &&
         a.shortText == b.shortText && a.count == b.count;
}

} // namespace vortch
