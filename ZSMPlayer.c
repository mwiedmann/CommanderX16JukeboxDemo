#include "ZSMPlayer.h"
#include <c64/kernalio.h>

void frame_wait(void)
{
	while (vera.ien & 0x40);
	while (!(vera.ien & 0x40));
}
void sfx_put(char index, char data)
{
	while (sfx.data & 0x80);
	sfx.index = index;
	__asm {
		nop
		nop
		nop
		nop
	}
	sfx.data = data;
}
void delay(unsigned t)
{
	while (t--)
		frame_wait();
}

static unsigned zsm_pos, zsm_wpos;
static char zsm_delay;
static volatile bool zsm_playing = false, zsm_reading = false, zsm_finished = true;
static char zsm_buffer[1024];

void zsm_play(void)
{
	if (zsm_delay)
		zsm_delay--;
	else
	{
		char		vc = vera.ctrl;
		vera.ctrl &= 0xfe;
		unsigned	va = vera.addr;
		char 		vh = vera.addrh;

		for (;;)
		{
			char c = zsm_buffer[zsm_pos++ & 0x3ff];
			if (c < 0x40)
			{
				vera.addr = (c & 0x3f) | 0xf9c0;
				vera.addrh = 0x01;
				vera.data0 = zsm_buffer[zsm_pos++ & 0x3ff];
			}
			else if (c == 0x40)
			{
				zsm_pos += zsm_buffer[zsm_pos & 0x3ff] & 0x3f;
			}
			else if (c < 0x80)
			{
				c &= 0x3f;
				for (char i = 0; i < c; i++)
				{
					sfx_put(zsm_buffer[(zsm_pos + 0) & 0x3ff], zsm_buffer[(zsm_pos + 1) & 0x3ff]);
					zsm_pos += 2;
				}
			}
			else if (c == 0x80)
			{
				zsm_finished = true;
				break;
			}
			else
			{
				zsm_delay = (c & 0x7f) - 1;
				break;
			}
		}

		vera.addr = va;
		vera.addrh = vh;
		vera.ctrl = vc;
	}
}
__interrupt void irq(void)
{
	if (zsm_playing && !zsm_finished)
		zsm_play();
}

static void* oirq;

__asm irqt
{
	jsr	irq
	jmp(oirq)
}

int zsm_fill(void)
{
	if (!zsm_reading)
		return -1;
	else if (zsm_wpos == zsm_pos + 1024)
		return 0;
	else if (krnio_chkin(2))
	{
		int n = 0;
		while (zsm_wpos != zsm_pos + 1024)
		{
			char ch = krnio_chrin();
			zsm_buffer[zsm_wpos++ & 0x3ff] = ch;
			n++;
			if (krnio_status())
			{
				zsm_reading = false;
				break;
			}
		}
		krnio_clrchn();

		if (!zsm_reading)
			krnio_close(2);

		return n;
	}
	else
		return -1;
}

bool zsm_check(void)
{
	return zsm_finished;
}


bool zsm_init(const char* fname)
{
	zsm_finished = true;

	if (zsm_reading)
	{
		krnio_close(2);
		zsm_reading = false;
	}

	zsm_pos = 0;
	zsm_wpos = 0;
	zsm_delay = 0;

	krnio_setnam(fname);	
	if (krnio_open(2, 8, 2))
	{	
		zsm_reading = true;
		zsm_fill();
		zsm_pos = 16;
		zsm_finished = false;

		return true;
	}
	else
		return false;
}

void zsm_irq_init(void)
{
	__asm
	{
		sei
	}

	oirq = *(void**)0x0314;
	*(void**)0x0314 = irqt;

	__asm
	{
		cli
	}
}

void zsm_irq_play(bool play)
{
	zsm_playing = play;
}


