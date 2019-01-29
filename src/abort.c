
int abort_flag = 0;
void set_abort_flag()
{
	abort_flag = 1;
}

int user_abort(void)
{ 
	if (abort_flag) {
		abort_flag = 0;
		return 1;
	} else 
		return 0;
} 
