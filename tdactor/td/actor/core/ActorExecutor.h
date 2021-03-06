#pragma once

#include "td/actor/core/ActorExecuteContext.h"
#include "td/actor/core/ActorInfo.h"
#include "td/actor/core/ActorLocker.h"
#include "td/actor/core/ActorMessage.h"
#include "td/actor/core/ActorSignals.h"
#include "td/actor/core/ActorState.h"
#include "td/actor/core/SchedulerContext.h"

#include "td/utils/format.h"

namespace td {
namespace actor {
namespace core {
class ActorExecutor {
 public:
  struct Options {
    Options &with_from_queue() {
      from_queue = true;
      return *this;
    }
    Options &with_has_poll(bool new_has_poll) {
      this->has_poll = new_has_poll;
      return *this;
    }
    bool from_queue{false};
    bool has_poll{false};
  };

  ActorExecutor(ActorInfo &actor_info, SchedulerDispatcher &dispatcher, Options options)
      : actor_info_(actor_info), dispatcher_(dispatcher), options_(options) {
    old_log_tag_ = LOG_TAG2;
    LOG_TAG2 = actor_info.get_name().c_str();
    start();
  }
  ActorExecutor(const ActorExecutor &) = delete;
  ActorExecutor &operator=(const ActorExecutor &) = delete;
  ActorExecutor(ActorExecutor &&other) = delete;
  ActorExecutor &operator=(ActorExecutor &&other) = delete;
  ~ActorExecutor() {
    finish();
    LOG_TAG2 = old_log_tag_;
  }

  // our best guess if actor is closed or not
  bool is_closed() {
    return flags().is_closed();
  }

  bool can_send_immediate() {
    return actor_locker_.own_lock() && !actor_execute_context_.has_immediate_flags() && actor_locker_.can_execute();
  }

  template <class F>
  void send_immediate(F &&f, uint64 link_token) {
    CHECK(can_send_immediate());
    if (is_closed()) {
      return;
    }
    actor_execute_context_.set_link_token(link_token);
    f();
  }

  void send_immediate(ActorMessage message);
  void send_immediate(ActorSignals signals);
  void send(ActorMessage message);
  void send(ActorSignals signals);

 private:
  ActorInfo &actor_info_;
  SchedulerDispatcher &dispatcher_;
  Options options_;
  ActorLocker actor_locker_{&actor_info_.state(), ActorLocker::Options()
                                                      .with_can_execute_paused(options_.from_queue)
                                                      .with_is_shared(!options_.has_poll)
                                                      .with_scheduler_id(dispatcher_.get_scheduler_id())};

  ActorExecuteContext actor_execute_context_{nullptr, actor_info_.get_alarm_timestamp()};
  ActorExecuteContext::Guard guard{&actor_execute_context_};

  ActorState::Flags flags_;
  ActorSignals pending_signals_;

  const char *old_log_tag_;

  ActorState::Flags &flags() {
    return flags_;
  }

  void start();
  void finish();

  bool flush_one(ActorSignals &signals);
  bool flush_one_signal(ActorSignals &signals);
  bool flush_one_message();
  void flush_context_flags();
};
}  // namespace core
}  // namespace actor
}  // namespace td
