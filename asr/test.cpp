// ASR - Digital Signal Processor
// Copyright (C) 2002-2013	Paul Ciarlo <paul.ciarlo@gmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.	 If not, see <http://www.gnu.org/licenses/>.

void testf()
{
	printf("testf\n");
#if TEST0
	char buf[128];
	char *f1 = buf;
	while ((int)f1 & 0xFF) ++f1;
	int *foo = (int*)f1, bar;
	foo[0] = 1;
	foo[1] = 2;
	foo[2] = 3;
	foo[3] = 4;
	foo[4] = 5;
	foo[5] = 6;
	foo[6] = 7;
	foo[7] = 8;

	__m128i a, b, *c;
	a.m128i_i32[0] = 1;
	a.m128i_i32[1] = 2;
	a.m128i_i32[2] = 3;
	a.m128i_i32[3] = 4;
	b = *(__m128i*)foo;
	1;
	__asm
	{
		;ldmxcsr 0x1f80
		movdqa xmm1, b

	}
	bar = 10;
	//c = (__m128i*)foo;
	//a = _mm_load_si128(c);
	//__asm
	//{
	//	movdqa xmm1, [foo]
	//}
	bar = 50;
	return;
#endif

#if TEST1
	FILE *output = fopen("foo.txt", "w");
#if !USE_SSE2 && !NON_SSE_INTS
		short *tmp = T_allocator_N_16ba<short, BUFFERSIZE>::alloc();
#else
		int *tmp = T_allocator_N_16ba<int, BUFFERSIZE>::alloc();
#endif
		float *buffer = T_allocator_N_16ba<float, BUFFERSIZE>::alloc(), *end;
		for (int i=0;i<BUFFERSIZE;++i)
		{
			tmp[i] = BUFFERSIZE/2 - i;
			fprintf(output, "%d ", tmp[i]);
		}
		fprintf(output, "\ncalling IOProcessor::LoadHelp\n");
		end = IOProcessor::LoadHelp(buffer, tmp);
		for (int i=0;i<BUFFERSIZE;++i)
		{
			fprintf(output, "%f ", buffer[i]);
		}
		fprintf(output, "\n%p %p %x\n", buffer, end, end-buffer);
	fclose(output);
#endif
}

void test_main()
{
	//chunk_T_T_size<int, 128> sthing;
	//printf("%d\n", sizeof(sthing._data));
	//printf("%x\n", _WIN32_WINNT);
	try{
		Tracer::hook(testf);
		testf();
	}catch(exception&e){
		throw e;
	}
	return 0;
}
