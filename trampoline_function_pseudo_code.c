trampoline_function_pseudo_code(){
	save_registers();
	rt=read_rt_data();
	tid=read_tid();
	thread_stats=read_thread_stats();
	update_ipi_received_trampoline(thread_stats,tid);
	current_lp=read_current_lp();
	LPS[current_lp]=read_LP_of_current_lp();

	msg_curr_executed=read_msg_in_execution();
	if(msg_curr_executed==NULL)
		goto L4;
	//msg_curr_executed not null
	count7++;
	if(msg_curr_executed->state != ANTI_MSG)
		goto L4;

	count8++;
	//msg_curr_executed not null and anti_msg
	goto L5;

	L4:
	//nothing 
	count0++;
	priority_message=read_priority_message();//don't re-read this value
	if(priority_message==NULL)
		goto L1;//count1++ other counter don't change

	//priority_msg not null
	count1++;
	state_priority_message=read_state_priority_message(priority_message);//don't re-read this value
	if(state_priority_message!=ANTI_MSG)
		goto L3;

	count2++;
	//priority_msg not null && state_priority_message==ANTI
	if(priority_message->monitor==BANANA)
		goto L1;
	else{//priority_msg not null && state_priority_message==ANTI && monitor!=banana
		goto L2;
	}

	count3++;
	L3:
	//priority_msg not null+state_priority_message!=ANTI
	if(state_priority_message!=NEW_EVT)
		goto L1;

	count4++;

	L2:
	//priority_msg not null && ( (state==ANTI && monitor!=banana) ||  state=NEW_EVT)
	bound=read_bound(LPS[current_lp]);//this value can be re-readed,only current_thread can change this value
	if(bound==NULL)
		goto L1;
	//priority_msg not null && bound!=NULL && ( (state==ANTI && monitor!=banana) ||  state=NEW_EVT)
	count5++;
	if(priority_message->timestamp>=bound->timestamp)
		goto L1;

	//priority_msg not null && bound!=NULL && ( (state==ANTI && monitor!=banana) ||  state=NEW_EVT) && (priority_message_is_really_priority==true)
	count6++;

	L5:
	goto prepare_to_cfv;
	prepare_to_cfv:
		do_cfv() __attribute__(no_return);//in this case we don't return
		
	goto L1://don't do cfv!!!
		restore_registers();
		dont_do_cfv();
		return;
}