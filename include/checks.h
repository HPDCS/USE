#ifndef CHECK_H
#define CHECK_H

void check_current_msg_is_in_future(unsigned int lid);
void check_silent_execution(unsigned short int old_state);
void check_stop_rollback(unsigned short int old_state,unsigned int lid);
void check_epoch_and_frame(unsigned int last_frame_event,unsigned int local_next_frame_event,unsigned int last_epoch_event,unsigned int local_next_epoch_event);
void check_events_state(unsigned short int old_state,msg_t*evt,msg_t*local_next_evt);
void check_queue_deliver_msgs();
void check_ScheduleNewEventFuture();
void check_ScheduleNewEventPast();
void check_CFV_TO_HANDLE_current_msg_null();
void check_CFV_TO_HANDLE_past();
void check_CFV_TO_HANDLE_future();
void check_thread_loop_after_executeEvent();
void check_tie_breaker_not_zero(unsigned int tie_breaker);
void check_thread_loop_after_fetch();
void check_CFV_INIT();
void check_CFV_TO_HANDLE();
void check_CFV_ALREADY_HANDLED();
void check_thread_loop_before_fetch();
void check_after_rollback();

#endif