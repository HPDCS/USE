trampoline_function_pseudo_code(){
	save_registers();
	rt=read_rt_data();
	tid=read_tid();
	update_ipi_received_trampoline(thread_stats,tid);
	current_lp=read_current_lp();
	LPS[current_lp]=read_LP_of_current_lp();
	count1++;
	priority_message=read_priority_message();//don't re-read this value
	if(priority_message==NULL)
		goto no_cfv;//label L1
	//priority_msg not null
	count2++;
	state_priority_message=read_state_priority_message(priority_message);//don't re-read this value

	if(state_priority_message!=ANTI_MSG)
		goto L2;
	count3++;
	//state_priority_message==ANTI
	if(priority_message->monitor==BANANA)
		goto no_cfv;
	//state_priority_message==ANTI && priority_message->monitor!=BANANA
	count4++;

	L2://case1:(state_priority_message!=ANTI_MSG) || case2:(state_priority_message==ANTI_MSG && priority_message->monitor==BANANA)
	if(state_priority_message!=NEW_EVT)
		goto no_cfv;//error if case2
	count5++;
	//execution state here???
	bound=read_bound(LPS[current_lp]);//this value can be re-readed,only current_thread can change this value
	if(bound==NULL)
		goto no_cfv;
	//execution state here???+bound!=NULL
	count6++;
	if(priority_message->timestamp>=bound->timestamp)
		goto no_cfv;

	//execution state here???+bound!=NULL+priority_message is really priority
	count7++;
	goto prepare_to_cfv;
	prepare_to_cfv:
		do_cfv();
		return;

	goto no_cfv://label L1
		restore_registers();
		return;
}