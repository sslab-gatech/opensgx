
simple-hello:     file format elf64-x86-64


Disassembly of section .text:

000000005000410c <enclave_main>:
    5000410c:	55                   	push   %rbp
    5000410d:	48 89 e5             	mov    %rsp,%rbp
    50004110:	53                   	push   %rbx
    50004111:	48 83 ec 18          	sub    $0x18,%rsp
    50004115:	48 8d 05 f2 18 00 00 	lea    0x18f2(%rip),%rax        # 50005a0e <strlen+0x1d3>
    5000411c:	48 89 45 e8          	mov    %rax,-0x18(%rbp)
    50004120:	48 8b 45 e8          	mov    -0x18(%rbp),%rax
    50004124:	48 89 c7             	mov    %rax,%rdi
    50004127:	e8 8e 04 00 00       	callq  500045ba <puts>
    5000412c:	b8 04 00 00 00       	mov    $0x4,%eax
    50004131:	ba 00 00 00 00       	mov    $0x0,%edx
    50004136:	48 89 d3             	mov    %rdx,%rbx
    50004139:	89 c0                	mov    %eax,%eax
    5000413b:	48 89 db             	mov    %rbx,%rbx
    5000413e:	0f 01 d7             	enclu  
    50004141:	48 83 c4 18          	add    $0x18,%rsp
    50004145:	5b                   	pop    %rbx
    50004146:	5d                   	pop    %rbp
    50004147:	c3                   	retq   

0000000050004148 <_enclu>:
    50004148:	55                   	push   %rbp
    50004149:	48 89 e5             	mov    %rsp,%rbp
    5000414c:	53                   	push   %rbx
    5000414d:	89 7d d4             	mov    %edi,-0x2c(%rbp)
    50004150:	48 89 75 c8          	mov    %rsi,-0x38(%rbp)
    50004154:	48 89 55 c0          	mov    %rdx,-0x40(%rbp)
    50004158:	48 89 4d b8          	mov    %rcx,-0x48(%rbp)
    5000415c:	4c 89 45 b0          	mov    %r8,-0x50(%rbp)
    50004160:	8b 45 d4             	mov    -0x2c(%rbp),%eax
    50004163:	48 8b 75 c8          	mov    -0x38(%rbp),%rsi
    50004167:	48 8b 4d c0          	mov    -0x40(%rbp),%rcx
    5000416b:	48 8b 55 b8          	mov    -0x48(%rbp),%rdx
    5000416f:	48 89 f3             	mov    %rsi,%rbx
    50004172:	0f 01 d7             	enclu  
    50004175:	48 89 de             	mov    %rbx,%rsi
    50004178:	89 45 d8             	mov    %eax,-0x28(%rbp)
    5000417b:	48 89 75 e0          	mov    %rsi,-0x20(%rbp)
    5000417f:	48 89 4d e8          	mov    %rcx,-0x18(%rbp)
    50004183:	48 89 55 f0          	mov    %rdx,-0x10(%rbp)
    50004187:	48 83 7d b0 00       	cmpq   $0x0,-0x50(%rbp)
    5000418c:	74 2e                	je     500041bc <_enclu+0x74>
    5000418e:	89 c0                	mov    %eax,%eax
    50004190:	48 89 db             	mov    %rbx,%rbx
    50004193:	48 89 c9             	mov    %rcx,%rcx
    50004196:	48 89 d2             	mov    %rdx,%rdx
    50004199:	48 89 de             	mov    %rbx,%rsi
    5000419c:	89 c7                	mov    %eax,%edi
    5000419e:	48 8b 45 b0          	mov    -0x50(%rbp),%rax
    500041a2:	89 38                	mov    %edi,(%rax)
    500041a4:	48 8b 45 b0          	mov    -0x50(%rbp),%rax
    500041a8:	48 89 70 08          	mov    %rsi,0x8(%rax)
    500041ac:	48 8b 45 b0          	mov    -0x50(%rbp),%rax
    500041b0:	48 89 48 10          	mov    %rcx,0x10(%rax)
    500041b4:	48 8b 45 b0          	mov    -0x50(%rbp),%rax
    500041b8:	48 89 50 18          	mov    %rdx,0x18(%rax)
    500041bc:	5b                   	pop    %rbx
    500041bd:	5d                   	pop    %rbp
    500041be:	c3                   	retq   

00000000500041bf <sgx_malloc_init>:
    500041bf:	55                   	push   %rbp
    500041c0:	48 89 e5             	mov    %rsp,%rbp
    500041c3:	53                   	push   %rbx
    500041c4:	b8 00 00 80 80       	mov    $0x80800000,%eax
    500041c9:	48 89 45 f0          	mov    %rax,-0x10(%rbp)
    500041cd:	48 8b 45 f0          	mov    -0x10(%rbp),%rax
    500041d1:	c7 80 3c 04 00 00 03 	movl   $0x3,0x43c(%rax)
    500041d8:	00 00 00 
    500041db:	48 8b 45 f0          	mov    -0x10(%rbp),%rax
    500041df:	c7 80 40 04 00 00 01 	movl   $0x1,0x440(%rax)
    500041e6:	00 00 00 
    500041e9:	48 8b 45 f0          	mov    -0x10(%rbp),%rax
    500041ed:	48 8b 40 08          	mov    0x8(%rax),%rax
    500041f1:	48 89 c2             	mov    %rax,%rdx
    500041f4:	b8 04 00 00 00       	mov    $0x4,%eax
    500041f9:	48 89 d3             	mov    %rdx,%rbx
    500041fc:	89 c0                	mov    %eax,%eax
    500041fe:	48 89 db             	mov    %rbx,%rbx
    50004201:	0f 01 d7             	enclu  
    50004204:	48 8b 45 f0          	mov    -0x10(%rbp),%rax
    50004208:	48 8b 40 18          	mov    0x18(%rax),%rax
    5000420c:	48 89 05 ed 1d 00 00 	mov    %rax,0x1ded(%rip)        # 50006000 <__bss_start>
    50004213:	48 8b 45 f0          	mov    -0x10(%rbp),%rax
    50004217:	48 8b 40 20          	mov    0x20(%rax),%rax
    5000421b:	48 89 05 e6 1d 00 00 	mov    %rax,0x1de6(%rip)        # 50006008 <heap_end>
    50004222:	48 8b 45 f0          	mov    -0x10(%rbp),%rax
    50004226:	48 8b 40 18          	mov    0x18(%rax),%rax
    5000422a:	48 89 05 e7 1d 00 00 	mov    %rax,0x1de7(%rip)        # 50006018 <managed_memory_start>
    50004231:	c7 05 d5 1d 00 00 01 	movl   $0x1,0x1dd5(%rip)        # 50006010 <has_initialized>
    50004238:	00 00 00 
    5000423b:	c7 05 db 1d 00 00 00 	movl   $0x0,0x1ddb(%rip)        # 50006020 <g_total_chunk>
    50004242:	00 00 00 
    50004245:	5b                   	pop    %rbx
    50004246:	5d                   	pop    %rbp
    50004247:	c3                   	retq   

0000000050004248 <free>:
    50004248:	55                   	push   %rbp
    50004249:	48 89 e5             	mov    %rsp,%rbp
    5000424c:	48 83 ec 20          	sub    $0x20,%rsp
    50004250:	48 89 7d e8          	mov    %rdi,-0x18(%rbp)
    50004254:	48 8b 45 e8          	mov    -0x18(%rbp),%rax
    50004258:	48 83 e8 40          	sub    $0x40,%rax
    5000425c:	48 89 45 f8          	mov    %rax,-0x8(%rbp)
    50004260:	48 8b 45 f8          	mov    -0x8(%rbp),%rax
    50004264:	c7 00 01 00 00 00    	movl   $0x1,(%rax)
    5000426a:	48 8b 45 f8          	mov    -0x8(%rbp),%rax
    5000426e:	8b 40 04             	mov    0x4(%rax),%eax
    50004271:	83 e8 08             	sub    $0x8,%eax
    50004274:	89 45 f4             	mov    %eax,-0xc(%rbp)
    50004277:	8b 55 f4             	mov    -0xc(%rbp),%edx
    5000427a:	48 8b 45 e8          	mov    -0x18(%rbp),%rax
    5000427e:	be 00 00 00 00       	mov    $0x0,%esi
    50004283:	48 89 c7             	mov    %rax,%rdi
    50004286:	e8 2c 14 00 00       	callq  500056b7 <memset>
    5000428b:	90                   	nop
    5000428c:	c9                   	leaveq 
    5000428d:	c3                   	retq   

000000005000428e <malloc>:
    5000428e:	55                   	push   %rbp
    5000428f:	48 89 e5             	mov    %rsp,%rbp
    50004292:	53                   	push   %rbx
    50004293:	48 81 ec 88 00 00 00 	sub    $0x88,%rsp
    5000429a:	48 89 bd 78 ff ff ff 	mov    %rdi,-0x88(%rbp)
    500042a1:	b8 00 00 80 80       	mov    $0x80800000,%eax
    500042a6:	48 89 45 d0          	mov    %rax,-0x30(%rbp)
    500042aa:	8b 05 60 1d 00 00    	mov    0x1d60(%rip),%eax        # 50006010 <has_initialized>
    500042b0:	85 c0                	test   %eax,%eax
    500042b2:	75 0a                	jne    500042be <malloc+0x30>
    500042b4:	b8 00 00 00 00       	mov    $0x0,%eax
    500042b9:	e8 01 ff ff ff       	callq  500041bf <sgx_malloc_init>
    500042be:	48 83 85 78 ff ff ff 	addq   $0x8,-0x88(%rbp)
    500042c5:	08 
    500042c6:	48 c7 45 e0 00 00 00 	movq   $0x0,-0x20(%rbp)
    500042cd:	00 
    500042ce:	48 8b 05 43 1d 00 00 	mov    0x1d43(%rip),%rax        # 50006018 <managed_memory_start>
    500042d5:	48 89 45 e8          	mov    %rax,-0x18(%rbp)
    500042d9:	8b 05 41 1d 00 00    	mov    0x1d41(%rip),%eax        # 50006020 <g_total_chunk>
    500042df:	89 45 dc             	mov    %eax,-0x24(%rbp)
    500042e2:	eb 4d                	jmp    50004331 <malloc+0xa3>
    500042e4:	48 8b 45 e8          	mov    -0x18(%rbp),%rax
    500042e8:	48 89 45 c8          	mov    %rax,-0x38(%rbp)
    500042ec:	48 8b 45 c8          	mov    -0x38(%rbp),%rax
    500042f0:	8b 00                	mov    (%rax),%eax
    500042f2:	85 c0                	test   %eax,%eax
    500042f4:	74 26                	je     5000431c <malloc+0x8e>
    500042f6:	48 8b 45 c8          	mov    -0x38(%rbp),%rax
    500042fa:	8b 40 04             	mov    0x4(%rax),%eax
    500042fd:	48 98                	cltq   
    500042ff:	48 3b 85 78 ff ff ff 	cmp    -0x88(%rbp),%rax
    50004306:	72 14                	jb     5000431c <malloc+0x8e>
    50004308:	48 8b 45 c8          	mov    -0x38(%rbp),%rax
    5000430c:	c7 00 00 00 00 00    	movl   $0x0,(%rax)
    50004312:	48 8b 45 e8          	mov    -0x18(%rbp),%rax
    50004316:	48 89 45 e0          	mov    %rax,-0x20(%rbp)
    5000431a:	eb 1b                	jmp    50004337 <malloc+0xa9>
    5000431c:	48 8b 45 c8          	mov    -0x38(%rbp),%rax
    50004320:	8b 40 04             	mov    0x4(%rax),%eax
    50004323:	48 98                	cltq   
    50004325:	48 c1 e0 03          	shl    $0x3,%rax
    50004329:	48 01 45 e8          	add    %rax,-0x18(%rbp)
    5000432d:	83 6d dc 01          	subl   $0x1,-0x24(%rbp)
    50004331:	83 7d dc 00          	cmpl   $0x0,-0x24(%rbp)
    50004335:	75 ad                	jne    500042e4 <malloc+0x56>
    50004337:	48 83 7d e0 00       	cmpq   $0x0,-0x20(%rbp)
    5000433c:	0f 85 9d 01 00 00    	jne    500044df <malloc+0x251>
    50004342:	48 8b 05 b7 1c 00 00 	mov    0x1cb7(%rip),%rax        # 50006000 <__bss_start>
    50004349:	48 89 45 c0          	mov    %rax,-0x40(%rbp)
    5000434d:	48 c7 45 b8 7f 00 00 	movq   $0x7f,-0x48(%rbp)
    50004354:	00 
    50004355:	48 8b 15 a4 1c 00 00 	mov    0x1ca4(%rip),%rdx        # 50006000 <__bss_start>
    5000435c:	48 8b 45 b8          	mov    -0x48(%rbp),%rax
    50004360:	48 01 c2             	add    %rax,%rdx
    50004363:	48 8b 85 78 ff ff ff 	mov    -0x88(%rbp),%rax
    5000436a:	48 01 c2             	add    %rax,%rdx
    5000436d:	48 8b 05 94 1c 00 00 	mov    0x1c94(%rip),%rax        # 50006008 <heap_end>
    50004374:	48 39 c2             	cmp    %rax,%rdx
    50004377:	0f 86 14 01 00 00    	jbe    50004491 <malloc+0x203>
    5000437d:	be 40 00 00 00       	mov    $0x40,%esi
    50004382:	bf 40 00 00 00       	mov    $0x40,%edi
    50004387:	e8 e0 01 00 00       	callq  5000456c <memalign>
    5000438c:	48 89 45 b0          	mov    %rax,-0x50(%rbp)
    50004390:	48 8b 45 b0          	mov    -0x50(%rbp),%rax
    50004394:	0f b6 10             	movzbl (%rax),%edx
    50004397:	83 ca 01             	or     $0x1,%edx
    5000439a:	88 10                	mov    %dl,(%rax)
    5000439c:	48 8b 45 b0          	mov    -0x50(%rbp),%rax
    500043a0:	0f b6 10             	movzbl (%rax),%edx
    500043a3:	83 ca 02             	or     $0x2,%edx
    500043a6:	88 10                	mov    %dl,(%rax)
    500043a8:	48 8b 45 b0          	mov    -0x50(%rbp),%rax
    500043ac:	0f b6 10             	movzbl (%rax),%edx
    500043af:	83 e2 fb             	and    $0xfffffffb,%edx
    500043b2:	88 10                	mov    %dl,(%rax)
    500043b4:	48 8b 45 b0          	mov    -0x50(%rbp),%rax
    500043b8:	0f b6 10             	movzbl (%rax),%edx
    500043bb:	83 ca 08             	or     $0x8,%edx
    500043be:	88 10                	mov    %dl,(%rax)
    500043c0:	48 8b 45 b0          	mov    -0x50(%rbp),%rax
    500043c4:	0f b6 10             	movzbl (%rax),%edx
    500043c7:	83 e2 ef             	and    $0xffffffef,%edx
    500043ca:	88 10                	mov    %dl,(%rax)
    500043cc:	48 8b 45 b0          	mov    -0x50(%rbp),%rax
    500043d0:	0f b6 10             	movzbl (%rax),%edx
    500043d3:	83 e2 1f             	and    $0x1f,%edx
    500043d6:	88 10                	mov    %dl,(%rax)
    500043d8:	48 8b 45 b0          	mov    -0x50(%rbp),%rax
    500043dc:	c6 40 01 02          	movb   $0x2,0x1(%rax)
    500043e0:	c7 45 d8 00 00 00 00 	movl   $0x0,-0x28(%rbp)
    500043e7:	c7 45 d8 00 00 00 00 	movl   $0x0,-0x28(%rbp)
    500043ee:	eb 12                	jmp    50004402 <malloc+0x174>
    500043f0:	48 8b 55 b0          	mov    -0x50(%rbp),%rdx
    500043f4:	8b 45 d8             	mov    -0x28(%rbp),%eax
    500043f7:	48 98                	cltq   
    500043f9:	c6 44 02 02 00       	movb   $0x0,0x2(%rdx,%rax,1)
    500043fe:	83 45 d8 01          	addl   $0x1,-0x28(%rbp)
    50004402:	83 7d d8 05          	cmpl   $0x5,-0x28(%rbp)
    50004406:	7e e8                	jle    500043f0 <malloc+0x162>
    50004408:	48 8b 45 d0          	mov    -0x30(%rbp),%rax
    5000440c:	c7 80 3c 04 00 00 03 	movl   $0x3,0x43c(%rax)
    50004413:	00 00 00 
    50004416:	48 8b 45 d0          	mov    -0x30(%rbp),%rax
    5000441a:	c7 80 40 04 00 00 02 	movl   $0x2,0x440(%rax)
    50004421:	00 00 00 
    50004424:	48 8b 45 d0          	mov    -0x30(%rbp),%rax
    50004428:	48 8b 40 08          	mov    0x8(%rax),%rax
    5000442c:	48 89 c2             	mov    %rax,%rdx
    5000442f:	b8 04 00 00 00       	mov    $0x4,%eax
    50004434:	48 89 d3             	mov    %rdx,%rbx
    50004437:	89 c0                	mov    %eax,%eax
    50004439:	48 89 db             	mov    %rbx,%rbx
    5000443c:	0f 01 d7             	enclu  
    5000443f:	48 8b 45 d0          	mov    -0x30(%rbp),%rax
    50004443:	8b 40 28             	mov    0x28(%rax),%eax
    50004446:	89 c0                	mov    %eax,%eax
    50004448:	48 89 45 a8          	mov    %rax,-0x58(%rbp)
    5000444c:	48 8b 45 b0          	mov    -0x50(%rbp),%rax
    50004450:	48 8d 4d 80          	lea    -0x80(%rbp),%rcx
    50004454:	48 8b 55 a8          	mov    -0x58(%rbp),%rdx
    50004458:	49 89 c8             	mov    %rcx,%r8
    5000445b:	b9 00 00 00 00       	mov    $0x0,%ecx
    50004460:	48 89 c6             	mov    %rax,%rsi
    50004463:	bf 05 00 00 00       	mov    $0x5,%edi
    50004468:	e8 db fc ff ff       	callq  50004148 <_enclu>
    5000446d:	8b 45 80             	mov    -0x80(%rbp),%eax
    50004470:	85 c0                	test   %eax,%eax
    50004472:	75 16                	jne    5000448a <malloc+0x1fc>
    50004474:	48 8b 05 8d 1b 00 00 	mov    0x1b8d(%rip),%rax        # 50006008 <heap_end>
    5000447b:	48 05 00 10 00 00    	add    $0x1000,%rax
    50004481:	48 89 05 80 1b 00 00 	mov    %rax,0x1b80(%rip)        # 50006008 <heap_end>
    50004488:	eb 07                	jmp    50004491 <malloc+0x203>
    5000448a:	b8 00 00 00 00       	mov    $0x0,%eax
    5000448f:	eb 57                	jmp    500044e8 <malloc+0x25a>
    50004491:	48 8b 55 c0          	mov    -0x40(%rbp),%rdx
    50004495:	48 8b 85 78 ff ff ff 	mov    -0x88(%rbp),%rax
    5000449c:	48 01 d0             	add    %rdx,%rax
    5000449f:	48 89 05 5a 1b 00 00 	mov    %rax,0x1b5a(%rip)        # 50006000 <__bss_start>
    500044a6:	48 8b 45 c0          	mov    -0x40(%rbp),%rax
    500044aa:	48 89 45 e0          	mov    %rax,-0x20(%rbp)
    500044ae:	48 8b 45 e0          	mov    -0x20(%rbp),%rax
    500044b2:	48 89 45 c8          	mov    %rax,-0x38(%rbp)
    500044b6:	48 8b 45 c8          	mov    -0x38(%rbp),%rax
    500044ba:	c7 00 00 00 00 00    	movl   $0x0,(%rax)
    500044c0:	48 8b 85 78 ff ff ff 	mov    -0x88(%rbp),%rax
    500044c7:	89 c2                	mov    %eax,%edx
    500044c9:	48 8b 45 c8          	mov    -0x38(%rbp),%rax
    500044cd:	89 50 04             	mov    %edx,0x4(%rax)
    500044d0:	8b 05 4a 1b 00 00    	mov    0x1b4a(%rip),%eax        # 50006020 <g_total_chunk>
    500044d6:	83 c0 01             	add    $0x1,%eax
    500044d9:	89 05 41 1b 00 00    	mov    %eax,0x1b41(%rip)        # 50006020 <g_total_chunk>
    500044df:	48 83 45 e0 40       	addq   $0x40,-0x20(%rbp)
    500044e4:	48 8b 45 e0          	mov    -0x20(%rbp),%rax
    500044e8:	48 81 c4 88 00 00 00 	add    $0x88,%rsp
    500044ef:	5b                   	pop    %rbx
    500044f0:	5d                   	pop    %rbp
    500044f1:	c3                   	retq   

00000000500044f2 <realloc>:
    500044f2:	55                   	push   %rbp
    500044f3:	48 89 e5             	mov    %rsp,%rbp
    500044f6:	48 83 ec 20          	sub    $0x20,%rsp
    500044fa:	48 89 7d e8          	mov    %rdi,-0x18(%rbp)
    500044fe:	48 89 75 e0          	mov    %rsi,-0x20(%rbp)
    50004502:	48 83 7d e8 00       	cmpq   $0x0,-0x18(%rbp)
    50004507:	75 0e                	jne    50004517 <realloc+0x25>
    50004509:	48 8b 45 e0          	mov    -0x20(%rbp),%rax
    5000450d:	48 89 c7             	mov    %rax,%rdi
    50004510:	e8 79 fd ff ff       	callq  5000428e <malloc>
    50004515:	eb 53                	jmp    5000456a <realloc+0x78>
    50004517:	48 83 7d e0 00       	cmpq   $0x0,-0x20(%rbp)
    5000451c:	75 13                	jne    50004531 <realloc+0x3f>
    5000451e:	48 8b 45 e8          	mov    -0x18(%rbp),%rax
    50004522:	48 89 c7             	mov    %rax,%rdi
    50004525:	e8 1e fd ff ff       	callq  50004248 <free>
    5000452a:	b8 00 00 00 00       	mov    $0x0,%eax
    5000452f:	eb 39                	jmp    5000456a <realloc+0x78>
    50004531:	48 8b 45 e0          	mov    -0x20(%rbp),%rax
    50004535:	48 89 c7             	mov    %rax,%rdi
    50004538:	e8 51 fd ff ff       	callq  5000428e <malloc>
    5000453d:	48 89 45 f8          	mov    %rax,-0x8(%rbp)
    50004541:	48 83 7d f8 00       	cmpq   $0x0,-0x8(%rbp)
    50004546:	74 1d                	je     50004565 <realloc+0x73>
    50004548:	48 8b 55 e0          	mov    -0x20(%rbp),%rdx
    5000454c:	48 8b 4d e8          	mov    -0x18(%rbp),%rcx
    50004550:	48 8b 45 f8          	mov    -0x8(%rbp),%rax
    50004554:	48 89 ce             	mov    %rcx,%rsi
    50004557:	48 89 c7             	mov    %rax,%rdi
    5000455a:	e8 cf 00 00 00       	callq  5000462e <memcpy>
    5000455f:	48 8b 45 f8          	mov    -0x8(%rbp),%rax
    50004563:	eb 05                	jmp    5000456a <realloc+0x78>
    50004565:	b8 00 00 00 00       	mov    $0x0,%eax
    5000456a:	c9                   	leaveq 
    5000456b:	c3                   	retq   

000000005000456c <memalign>:
    5000456c:	55                   	push   %rbp
    5000456d:	48 89 e5             	mov    %rsp,%rbp
    50004570:	48 83 ec 20          	sub    $0x20,%rsp
    50004574:	48 89 7d e8          	mov    %rdi,-0x18(%rbp)
    50004578:	48 89 75 e0          	mov    %rsi,-0x20(%rbp)
    5000457c:	48 8b 55 e8          	mov    -0x18(%rbp),%rdx
    50004580:	48 8b 45 e0          	mov    -0x20(%rbp),%rax
    50004584:	48 01 d0             	add    %rdx,%rax
    50004587:	48 83 e8 01          	sub    $0x1,%rax
    5000458b:	48 89 c7             	mov    %rax,%rdi
    5000458e:	e8 fb fc ff ff       	callq  5000428e <malloc>
    50004593:	48 89 45 f8          	mov    %rax,-0x8(%rbp)
    50004597:	48 8b 55 f8          	mov    -0x8(%rbp),%rdx
    5000459b:	48 8b 45 e8          	mov    -0x18(%rbp),%rax
    5000459f:	48 01 d0             	add    %rdx,%rax
    500045a2:	48 8d 50 ff          	lea    -0x1(%rax),%rdx
    500045a6:	48 8b 45 e8          	mov    -0x18(%rbp),%rax
    500045aa:	48 f7 d8             	neg    %rax
    500045ad:	48 21 d0             	and    %rdx,%rax
    500045b0:	48 89 45 f0          	mov    %rax,-0x10(%rbp)
    500045b4:	48 8b 45 f0          	mov    -0x10(%rbp),%rax
    500045b8:	c9                   	leaveq 
    500045b9:	c3                   	retq   

00000000500045ba <puts>:
    500045ba:	55                   	push   %rbp
    500045bb:	48 89 e5             	mov    %rsp,%rbp
    500045be:	53                   	push   %rbx
    500045bf:	48 83 ec 28          	sub    $0x28,%rsp
    500045c3:	48 89 7d d8          	mov    %rdi,-0x28(%rbp)
    500045c7:	48 8b 45 d8          	mov    -0x28(%rbp),%rax
    500045cb:	48 89 c7             	mov    %rax,%rdi
    500045ce:	e8 68 12 00 00       	callq  5000583b <strlen>
    500045d3:	48 89 45 e8          	mov    %rax,-0x18(%rbp)
    500045d7:	b8 00 00 80 80       	mov    $0x80800000,%eax
    500045dc:	48 89 45 e0          	mov    %rax,-0x20(%rbp)
    500045e0:	48 8b 45 e0          	mov    -0x20(%rbp),%rax
    500045e4:	c7 80 3c 04 00 00 01 	movl   $0x1,0x43c(%rax)
    500045eb:	00 00 00 
    500045ee:	48 8b 45 e0          	mov    -0x20(%rbp),%rax
    500045f2:	48 8d 88 5c 04 00 00 	lea    0x45c(%rax),%rcx
    500045f9:	48 8b 55 e8          	mov    -0x18(%rbp),%rdx
    500045fd:	48 8b 45 d8          	mov    -0x28(%rbp),%rax
    50004601:	48 89 c6             	mov    %rax,%rsi
    50004604:	48 89 cf             	mov    %rcx,%rdi
    50004607:	e8 22 00 00 00       	callq  5000462e <memcpy>
    5000460c:	48 8b 45 e0          	mov    -0x20(%rbp),%rax
    50004610:	48 8b 40 08          	mov    0x8(%rax),%rax
    50004614:	48 89 c2             	mov    %rax,%rdx
    50004617:	b8 04 00 00 00       	mov    $0x4,%eax
    5000461c:	48 89 d3             	mov    %rdx,%rbx
    5000461f:	89 c0                	mov    %eax,%eax
    50004621:	48 89 db             	mov    %rbx,%rbx
    50004624:	0f 01 d7             	enclu  
    50004627:	48 83 c4 28          	add    $0x28,%rsp
    5000462b:	5b                   	pop    %rbx
    5000462c:	5d                   	pop    %rbp
    5000462d:	c3                   	retq   

000000005000462e <memcpy>:
    5000462e:	55                   	push   %rbp
    5000462f:	48 89 e5             	mov    %rsp,%rbp
    50004632:	53                   	push   %rbx
    50004633:	48 89 7d f0          	mov    %rdi,-0x10(%rbp)
    50004637:	48 89 75 e8          	mov    %rsi,-0x18(%rbp)
    5000463b:	48 89 55 e0          	mov    %rdx,-0x20(%rbp)
    5000463f:	48 8b 45 f0          	mov    -0x10(%rbp),%rax
    50004643:	48 8b 55 e8          	mov    -0x18(%rbp),%rdx
    50004647:	48 8b 4d e0          	mov    -0x20(%rbp),%rcx
    5000464b:	48 89 d3             	mov    %rdx,%rbx
    5000464e:	48 89 c7             	mov    %rax,%rdi
    50004651:	48 89 de             	mov    %rbx,%rsi
    50004654:	89 c9                	mov    %ecx,%ecx
    50004656:	f3 a4                	rep movsb %ds:(%rsi),%es:(%rdi)
    50004658:	48 8b 45 f0          	mov    -0x10(%rbp),%rax
    5000465c:	5b                   	pop    %rbx
    5000465d:	5d                   	pop    %rbp
    5000465e:	c3                   	retq   

000000005000465f <memmove>:
    5000465f:	55                   	push   %rbp
    50004660:	48 89 e5             	mov    %rsp,%rbp
    50004663:	53                   	push   %rbx
    50004664:	48 89 7d f0          	mov    %rdi,-0x10(%rbp)
    50004668:	48 89 75 e8          	mov    %rsi,-0x18(%rbp)
    5000466c:	48 89 55 e0          	mov    %rdx,-0x20(%rbp)
    50004670:	48 8b 45 f0          	mov    -0x10(%rbp),%rax
    50004674:	48 8b 55 e8          	mov    -0x18(%rbp),%rdx
    50004678:	48 8b 4d e0          	mov    -0x20(%rbp),%rcx
    5000467c:	48 89 d3             	mov    %rdx,%rbx
    5000467f:	48 89 c7             	mov    %rax,%rdi
    50004682:	48 89 de             	mov    %rbx,%rsi
    50004685:	89 c9                	mov    %ecx,%ecx
    50004687:	f3 a4                	rep movsb %ds:(%rsi),%es:(%rdi)
    50004689:	48 8b 45 f0          	mov    -0x10(%rbp),%rax
    5000468d:	5b                   	pop    %rbx
    5000468e:	5d                   	pop    %rbp
    5000468f:	c3                   	retq   

0000000050004690 <time>:
    50004690:	55                   	push   %rbp
    50004691:	48 89 e5             	mov    %rsp,%rbp
    50004694:	53                   	push   %rbx
    50004695:	48 83 ec 18          	sub    $0x18,%rsp
    50004699:	48 89 7d e0          	mov    %rdi,-0x20(%rbp)
    5000469d:	b8 00 00 80 80       	mov    $0x80800000,%eax
    500046a2:	48 89 45 f0          	mov    %rax,-0x10(%rbp)
    500046a6:	48 8b 45 f0          	mov    -0x10(%rbp),%rax
    500046aa:	c7 80 3c 04 00 00 09 	movl   $0x9,0x43c(%rax)
    500046b1:	00 00 00 
    500046b4:	48 8b 45 f0          	mov    -0x10(%rbp),%rax
    500046b8:	48 8b 40 08          	mov    0x8(%rax),%rax
    500046bc:	48 89 c2             	mov    %rax,%rdx
    500046bf:	b8 04 00 00 00       	mov    $0x4,%eax
    500046c4:	48 89 d3             	mov    %rdx,%rbx
    500046c7:	89 c0                	mov    %eax,%eax
    500046c9:	48 89 db             	mov    %rbx,%rbx
    500046cc:	0f 01 d7             	enclu  
    500046cf:	48 83 7d e0 00       	cmpq   $0x0,-0x20(%rbp)
    500046d4:	74 1f                	je     500046f5 <time+0x65>
    500046d6:	48 8b 45 f0          	mov    -0x10(%rbp),%rax
    500046da:	48 8d 88 5c 04 00 00 	lea    0x45c(%rax),%rcx
    500046e1:	48 8b 45 e0          	mov    -0x20(%rbp),%rax
    500046e5:	ba 08 00 00 00       	mov    $0x8,%edx
    500046ea:	48 89 ce             	mov    %rcx,%rsi
    500046ed:	48 89 c7             	mov    %rax,%rdi
    500046f0:	e8 39 ff ff ff       	callq  5000462e <memcpy>
    500046f5:	48 8b 45 f0          	mov    -0x10(%rbp),%rax
    500046f9:	8b 80 38 04 00 00    	mov    0x438(%rax),%eax
    500046ff:	89 c0                	mov    %eax,%eax
    50004701:	48 83 c4 18          	add    $0x18,%rsp
    50004705:	5b                   	pop    %rbx
    50004706:	5d                   	pop    %rbp
    50004707:	c3                   	retq   

0000000050004708 <write>:
    50004708:	55                   	push   %rbp
    50004709:	48 89 e5             	mov    %rsp,%rbp
    5000470c:	53                   	push   %rbx
    5000470d:	48 83 ec 28          	sub    $0x28,%rsp
    50004711:	89 7d e4             	mov    %edi,-0x1c(%rbp)
    50004714:	48 89 75 d8          	mov    %rsi,-0x28(%rbp)
    50004718:	48 89 55 d0          	mov    %rdx,-0x30(%rbp)
    5000471c:	b8 00 00 80 80       	mov    $0x80800000,%eax
    50004721:	48 89 45 e8          	mov    %rax,-0x18(%rbp)
    50004725:	c7 45 f0 00 00 00 00 	movl   $0x0,-0x10(%rbp)
    5000472c:	e9 ab 00 00 00       	jmpq   500047dc <write+0xd4>
    50004731:	48 8b 45 e8          	mov    -0x18(%rbp),%rax
    50004735:	c7 80 3c 04 00 00 07 	movl   $0x7,0x43c(%rax)
    5000473c:	00 00 00 
    5000473f:	48 8b 45 e8          	mov    -0x18(%rbp),%rax
    50004743:	8b 55 e4             	mov    -0x1c(%rbp),%edx
    50004746:	89 90 50 04 00 00    	mov    %edx,0x450(%rax)
    5000474c:	8b 45 f0             	mov    -0x10(%rbp),%eax
    5000474f:	48 98                	cltq   
    50004751:	48 8b 55 d0          	mov    -0x30(%rbp),%rdx
    50004755:	48 c1 ea 09          	shr    $0x9,%rdx
    50004759:	48 39 d0             	cmp    %rdx,%rax
    5000475c:	75 1f                	jne    5000477d <write+0x75>
    5000475e:	48 8b 45 d0          	mov    -0x30(%rbp),%rax
    50004762:	89 c2                	mov    %eax,%edx
    50004764:	89 d0                	mov    %edx,%eax
    50004766:	c1 f8 1f             	sar    $0x1f,%eax
    50004769:	c1 e8 17             	shr    $0x17,%eax
    5000476c:	01 c2                	add    %eax,%edx
    5000476e:	81 e2 ff 01 00 00    	and    $0x1ff,%edx
    50004774:	29 c2                	sub    %eax,%edx
    50004776:	89 d0                	mov    %edx,%eax
    50004778:	89 45 f4             	mov    %eax,-0xc(%rbp)
    5000477b:	eb 07                	jmp    50004784 <write+0x7c>
    5000477d:	c7 45 f4 00 02 00 00 	movl   $0x200,-0xc(%rbp)
    50004784:	48 8b 45 e8          	mov    -0x18(%rbp),%rax
    50004788:	8b 55 f4             	mov    -0xc(%rbp),%edx
    5000478b:	89 90 54 04 00 00    	mov    %edx,0x454(%rax)
    50004791:	8b 45 f4             	mov    -0xc(%rbp),%eax
    50004794:	48 98                	cltq   
    50004796:	8b 55 f0             	mov    -0x10(%rbp),%edx
    50004799:	c1 e2 09             	shl    $0x9,%edx
    5000479c:	48 63 ca             	movslq %edx,%rcx
    5000479f:	48 8b 55 d8          	mov    -0x28(%rbp),%rdx
    500047a3:	48 8d 34 11          	lea    (%rcx,%rdx,1),%rsi
    500047a7:	48 8b 55 e8          	mov    -0x18(%rbp),%rdx
    500047ab:	48 8d 8a 5c 04 00 00 	lea    0x45c(%rdx),%rcx
    500047b2:	48 89 c2             	mov    %rax,%rdx
    500047b5:	48 89 cf             	mov    %rcx,%rdi
    500047b8:	e8 71 fe ff ff       	callq  5000462e <memcpy>
    500047bd:	48 8b 45 e8          	mov    -0x18(%rbp),%rax
    500047c1:	48 8b 40 08          	mov    0x8(%rax),%rax
    500047c5:	48 89 c2             	mov    %rax,%rdx
    500047c8:	b8 04 00 00 00       	mov    $0x4,%eax
    500047cd:	48 89 d3             	mov    %rdx,%rbx
    500047d0:	89 c0                	mov    %eax,%eax
    500047d2:	48 89 db             	mov    %rbx,%rbx
    500047d5:	0f 01 d7             	enclu  
    500047d8:	83 45 f0 01          	addl   $0x1,-0x10(%rbp)
    500047dc:	8b 45 f0             	mov    -0x10(%rbp),%eax
    500047df:	48 98                	cltq   
    500047e1:	48 8b 55 d0          	mov    -0x30(%rbp),%rdx
    500047e5:	48 c1 ea 09          	shr    $0x9,%rdx
    500047e9:	48 83 c2 01          	add    $0x1,%rdx
    500047ed:	48 39 d0             	cmp    %rdx,%rax
    500047f0:	0f 82 3b ff ff ff    	jb     50004731 <write+0x29>
    500047f6:	48 8b 45 e8          	mov    -0x18(%rbp),%rax
    500047fa:	8b 80 30 04 00 00    	mov    0x430(%rax),%eax
    50004800:	48 98                	cltq   
    50004802:	48 83 c4 28          	add    $0x28,%rsp
    50004806:	5b                   	pop    %rbx
    50004807:	5d                   	pop    %rbp
    50004808:	c3                   	retq   

0000000050004809 <read>:
    50004809:	55                   	push   %rbp
    5000480a:	48 89 e5             	mov    %rsp,%rbp
    5000480d:	53                   	push   %rbx
    5000480e:	48 83 ec 28          	sub    $0x28,%rsp
    50004812:	89 7d e4             	mov    %edi,-0x1c(%rbp)
    50004815:	48 89 75 d8          	mov    %rsi,-0x28(%rbp)
    50004819:	48 89 55 d0          	mov    %rdx,-0x30(%rbp)
    5000481d:	b8 00 00 80 80       	mov    $0x80800000,%eax
    50004822:	48 89 45 e8          	mov    %rax,-0x18(%rbp)
    50004826:	c7 45 f0 00 00 00 00 	movl   $0x0,-0x10(%rbp)
    5000482d:	e9 a7 00 00 00       	jmpq   500048d9 <read+0xd0>
    50004832:	48 8b 45 e8          	mov    -0x18(%rbp),%rax
    50004836:	c7 80 3c 04 00 00 06 	movl   $0x6,0x43c(%rax)
    5000483d:	00 00 00 
    50004840:	48 8b 45 e8          	mov    -0x18(%rbp),%rax
    50004844:	8b 55 e4             	mov    -0x1c(%rbp),%edx
    50004847:	89 90 50 04 00 00    	mov    %edx,0x450(%rax)
    5000484d:	8b 45 f0             	mov    -0x10(%rbp),%eax
    50004850:	48 98                	cltq   
    50004852:	48 8b 55 d0          	mov    -0x30(%rbp),%rdx
    50004856:	48 c1 ea 09          	shr    $0x9,%rdx
    5000485a:	48 39 d0             	cmp    %rdx,%rax
    5000485d:	75 1f                	jne    5000487e <read+0x75>
    5000485f:	48 8b 45 d0          	mov    -0x30(%rbp),%rax
    50004863:	89 c2                	mov    %eax,%edx
    50004865:	89 d0                	mov    %edx,%eax
    50004867:	c1 f8 1f             	sar    $0x1f,%eax
    5000486a:	c1 e8 17             	shr    $0x17,%eax
    5000486d:	01 c2                	add    %eax,%edx
    5000486f:	81 e2 ff 01 00 00    	and    $0x1ff,%edx
    50004875:	29 c2                	sub    %eax,%edx
    50004877:	89 d0                	mov    %edx,%eax
    50004879:	89 45 f4             	mov    %eax,-0xc(%rbp)
    5000487c:	eb 07                	jmp    50004885 <read+0x7c>
    5000487e:	c7 45 f4 00 02 00 00 	movl   $0x200,-0xc(%rbp)
    50004885:	48 8b 45 e8          	mov    -0x18(%rbp),%rax
    50004889:	8b 55 f4             	mov    -0xc(%rbp),%edx
    5000488c:	89 90 54 04 00 00    	mov    %edx,0x454(%rax)
    50004892:	48 8b 45 e8          	mov    -0x18(%rbp),%rax
    50004896:	48 8b 40 08          	mov    0x8(%rax),%rax
    5000489a:	48 89 c2             	mov    %rax,%rdx
    5000489d:	b8 04 00 00 00       	mov    $0x4,%eax
    500048a2:	48 89 d3             	mov    %rdx,%rbx
    500048a5:	89 c0                	mov    %eax,%eax
    500048a7:	48 89 db             	mov    %rbx,%rbx
    500048aa:	0f 01 d7             	enclu  
    500048ad:	8b 45 f4             	mov    -0xc(%rbp),%eax
    500048b0:	48 98                	cltq   
    500048b2:	48 8b 55 e8          	mov    -0x18(%rbp),%rdx
    500048b6:	48 8d 72 30          	lea    0x30(%rdx),%rsi
    500048ba:	8b 55 f0             	mov    -0x10(%rbp),%edx
    500048bd:	c1 e2 09             	shl    $0x9,%edx
    500048c0:	48 63 ca             	movslq %edx,%rcx
    500048c3:	48 8b 55 d8          	mov    -0x28(%rbp),%rdx
    500048c7:	48 01 d1             	add    %rdx,%rcx
    500048ca:	48 89 c2             	mov    %rax,%rdx
    500048cd:	48 89 cf             	mov    %rcx,%rdi
    500048d0:	e8 59 fd ff ff       	callq  5000462e <memcpy>
    500048d5:	83 45 f0 01          	addl   $0x1,-0x10(%rbp)
    500048d9:	8b 45 f0             	mov    -0x10(%rbp),%eax
    500048dc:	48 98                	cltq   
    500048de:	48 8b 55 d0          	mov    -0x30(%rbp),%rdx
    500048e2:	48 c1 ea 09          	shr    $0x9,%rdx
    500048e6:	48 83 c2 01          	add    $0x1,%rdx
    500048ea:	48 39 d0             	cmp    %rdx,%rax
    500048ed:	0f 82 3f ff ff ff    	jb     50004832 <read+0x29>
    500048f3:	48 8b 45 e8          	mov    -0x18(%rbp),%rax
    500048f7:	8b 80 30 04 00 00    	mov    0x430(%rax),%eax
    500048fd:	48 98                	cltq   
    500048ff:	48 83 c4 28          	add    $0x28,%rsp
    50004903:	5b                   	pop    %rbx
    50004904:	5d                   	pop    %rbp
    50004905:	c3                   	retq   

0000000050004906 <close>:
    50004906:	55                   	push   %rbp
    50004907:	48 89 e5             	mov    %rsp,%rbp
    5000490a:	53                   	push   %rbx
    5000490b:	89 7d e4             	mov    %edi,-0x1c(%rbp)
    5000490e:	b8 00 00 80 80       	mov    $0x80800000,%eax
    50004913:	48 89 45 f0          	mov    %rax,-0x10(%rbp)
    50004917:	48 8b 45 f0          	mov    -0x10(%rbp),%rax
    5000491b:	c7 80 3c 04 00 00 08 	movl   $0x8,0x43c(%rax)
    50004922:	00 00 00 
    50004925:	48 8b 45 f0          	mov    -0x10(%rbp),%rax
    50004929:	8b 55 e4             	mov    -0x1c(%rbp),%edx
    5000492c:	89 90 50 04 00 00    	mov    %edx,0x450(%rax)
    50004932:	48 8b 45 f0          	mov    -0x10(%rbp),%rax
    50004936:	48 8b 40 08          	mov    0x8(%rax),%rax
    5000493a:	48 89 c2             	mov    %rax,%rdx
    5000493d:	b8 04 00 00 00       	mov    $0x4,%eax
    50004942:	48 89 d3             	mov    %rdx,%rbx
    50004945:	89 c0                	mov    %eax,%eax
    50004947:	48 89 db             	mov    %rbx,%rbx
    5000494a:	0f 01 d7             	enclu  
    5000494d:	48 8b 45 f0          	mov    -0x10(%rbp),%rax
    50004951:	8b 80 30 04 00 00    	mov    0x430(%rax),%eax
    50004957:	5b                   	pop    %rbx
    50004958:	5d                   	pop    %rbp
    50004959:	c3                   	retq   

000000005000495a <socket>:
    5000495a:	55                   	push   %rbp
    5000495b:	48 89 e5             	mov    %rsp,%rbp
    5000495e:	53                   	push   %rbx
    5000495f:	89 7d e4             	mov    %edi,-0x1c(%rbp)
    50004962:	89 75 e0             	mov    %esi,-0x20(%rbp)
    50004965:	89 55 dc             	mov    %edx,-0x24(%rbp)
    50004968:	b8 00 00 80 80       	mov    $0x80800000,%eax
    5000496d:	48 89 45 f0          	mov    %rax,-0x10(%rbp)
    50004971:	48 8b 45 f0          	mov    -0x10(%rbp),%rax
    50004975:	c7 80 3c 04 00 00 0a 	movl   $0xa,0x43c(%rax)
    5000497c:	00 00 00 
    5000497f:	48 8b 45 f0          	mov    -0x10(%rbp),%rax
    50004983:	8b 55 e4             	mov    -0x1c(%rbp),%edx
    50004986:	89 90 50 04 00 00    	mov    %edx,0x450(%rax)
    5000498c:	48 8b 45 f0          	mov    -0x10(%rbp),%rax
    50004990:	8b 55 e0             	mov    -0x20(%rbp),%edx
    50004993:	89 90 54 04 00 00    	mov    %edx,0x454(%rax)
    50004999:	48 8b 45 f0          	mov    -0x10(%rbp),%rax
    5000499d:	8b 55 dc             	mov    -0x24(%rbp),%edx
    500049a0:	89 90 58 04 00 00    	mov    %edx,0x458(%rax)
    500049a6:	48 8b 45 f0          	mov    -0x10(%rbp),%rax
    500049aa:	48 8b 40 08          	mov    0x8(%rax),%rax
    500049ae:	48 89 c2             	mov    %rax,%rdx
    500049b1:	b8 04 00 00 00       	mov    $0x4,%eax
    500049b6:	48 89 d3             	mov    %rdx,%rbx
    500049b9:	89 c0                	mov    %eax,%eax
    500049bb:	48 89 db             	mov    %rbx,%rbx
    500049be:	0f 01 d7             	enclu  
    500049c1:	48 8b 45 f0          	mov    -0x10(%rbp),%rax
    500049c5:	8b 80 30 04 00 00    	mov    0x430(%rax),%eax
    500049cb:	5b                   	pop    %rbx
    500049cc:	5d                   	pop    %rbp
    500049cd:	c3                   	retq   

00000000500049ce <bind>:
    500049ce:	55                   	push   %rbp
    500049cf:	48 89 e5             	mov    %rsp,%rbp
    500049d2:	53                   	push   %rbx
    500049d3:	48 83 ec 20          	sub    $0x20,%rsp
    500049d7:	89 7d e4             	mov    %edi,-0x1c(%rbp)
    500049da:	48 89 75 d8          	mov    %rsi,-0x28(%rbp)
    500049de:	89 55 e0             	mov    %edx,-0x20(%rbp)
    500049e1:	b8 00 00 80 80       	mov    $0x80800000,%eax
    500049e6:	48 89 45 f0          	mov    %rax,-0x10(%rbp)
    500049ea:	48 8b 45 f0          	mov    -0x10(%rbp),%rax
    500049ee:	c7 80 3c 04 00 00 0b 	movl   $0xb,0x43c(%rax)
    500049f5:	00 00 00 
    500049f8:	48 8b 45 f0          	mov    -0x10(%rbp),%rax
    500049fc:	8b 55 e4             	mov    -0x1c(%rbp),%edx
    500049ff:	89 90 50 04 00 00    	mov    %edx,0x450(%rax)
    50004a05:	8b 55 e0             	mov    -0x20(%rbp),%edx
    50004a08:	48 8b 45 f0          	mov    -0x10(%rbp),%rax
    50004a0c:	48 8d 88 5c 04 00 00 	lea    0x45c(%rax),%rcx
    50004a13:	48 8b 45 d8          	mov    -0x28(%rbp),%rax
    50004a17:	48 89 c6             	mov    %rax,%rsi
    50004a1a:	48 89 cf             	mov    %rcx,%rdi
    50004a1d:	e8 0c fc ff ff       	callq  5000462e <memcpy>
    50004a22:	8b 55 e0             	mov    -0x20(%rbp),%edx
    50004a25:	48 8b 45 f0          	mov    -0x10(%rbp),%rax
    50004a29:	89 90 54 04 00 00    	mov    %edx,0x454(%rax)
    50004a2f:	48 8b 45 f0          	mov    -0x10(%rbp),%rax
    50004a33:	48 8b 40 08          	mov    0x8(%rax),%rax
    50004a37:	48 89 c2             	mov    %rax,%rdx
    50004a3a:	b8 04 00 00 00       	mov    $0x4,%eax
    50004a3f:	48 89 d3             	mov    %rdx,%rbx
    50004a42:	89 c0                	mov    %eax,%eax
    50004a44:	48 89 db             	mov    %rbx,%rbx
    50004a47:	0f 01 d7             	enclu  
    50004a4a:	48 8b 45 f0          	mov    -0x10(%rbp),%rax
    50004a4e:	8b 80 30 04 00 00    	mov    0x430(%rax),%eax
    50004a54:	48 83 c4 20          	add    $0x20,%rsp
    50004a58:	5b                   	pop    %rbx
    50004a59:	5d                   	pop    %rbp
    50004a5a:	c3                   	retq   

0000000050004a5b <listen>:
    50004a5b:	55                   	push   %rbp
    50004a5c:	48 89 e5             	mov    %rsp,%rbp
    50004a5f:	53                   	push   %rbx
    50004a60:	89 7d e4             	mov    %edi,-0x1c(%rbp)
    50004a63:	89 75 e0             	mov    %esi,-0x20(%rbp)
    50004a66:	b8 00 00 80 80       	mov    $0x80800000,%eax
    50004a6b:	48 89 45 f0          	mov    %rax,-0x10(%rbp)
    50004a6f:	48 8b 45 f0          	mov    -0x10(%rbp),%rax
    50004a73:	c7 80 3c 04 00 00 0c 	movl   $0xc,0x43c(%rax)
    50004a7a:	00 00 00 
    50004a7d:	48 8b 45 f0          	mov    -0x10(%rbp),%rax
    50004a81:	8b 55 e4             	mov    -0x1c(%rbp),%edx
    50004a84:	89 90 50 04 00 00    	mov    %edx,0x450(%rax)
    50004a8a:	48 8b 45 f0          	mov    -0x10(%rbp),%rax
    50004a8e:	8b 55 e0             	mov    -0x20(%rbp),%edx
    50004a91:	89 90 54 04 00 00    	mov    %edx,0x454(%rax)
    50004a97:	48 8b 45 f0          	mov    -0x10(%rbp),%rax
    50004a9b:	48 8b 40 08          	mov    0x8(%rax),%rax
    50004a9f:	48 89 c2             	mov    %rax,%rdx
    50004aa2:	b8 04 00 00 00       	mov    $0x4,%eax
    50004aa7:	48 89 d3             	mov    %rdx,%rbx
    50004aaa:	89 c0                	mov    %eax,%eax
    50004aac:	48 89 db             	mov    %rbx,%rbx
    50004aaf:	0f 01 d7             	enclu  
    50004ab2:	48 8b 45 f0          	mov    -0x10(%rbp),%rax
    50004ab6:	8b 80 30 04 00 00    	mov    0x430(%rax),%eax
    50004abc:	5b                   	pop    %rbx
    50004abd:	5d                   	pop    %rbp
    50004abe:	c3                   	retq   

0000000050004abf <accept>:
    50004abf:	55                   	push   %rbp
    50004ac0:	48 89 e5             	mov    %rsp,%rbp
    50004ac3:	53                   	push   %rbx
    50004ac4:	48 83 ec 28          	sub    $0x28,%rsp
    50004ac8:	89 7d e4             	mov    %edi,-0x1c(%rbp)
    50004acb:	48 89 75 d8          	mov    %rsi,-0x28(%rbp)
    50004acf:	48 89 55 d0          	mov    %rdx,-0x30(%rbp)
    50004ad3:	b8 00 00 80 80       	mov    $0x80800000,%eax
    50004ad8:	48 89 45 f0          	mov    %rax,-0x10(%rbp)
    50004adc:	48 8b 45 f0          	mov    -0x10(%rbp),%rax
    50004ae0:	c7 80 3c 04 00 00 0d 	movl   $0xd,0x43c(%rax)
    50004ae7:	00 00 00 
    50004aea:	48 8b 45 f0          	mov    -0x10(%rbp),%rax
    50004aee:	8b 55 e4             	mov    -0x1c(%rbp),%edx
    50004af1:	89 90 50 04 00 00    	mov    %edx,0x450(%rax)
    50004af7:	48 8b 45 f0          	mov    -0x10(%rbp),%rax
    50004afb:	48 8b 40 08          	mov    0x8(%rax),%rax
    50004aff:	48 89 c2             	mov    %rax,%rdx
    50004b02:	b8 04 00 00 00       	mov    $0x4,%eax
    50004b07:	48 89 d3             	mov    %rdx,%rbx
    50004b0a:	89 c0                	mov    %eax,%eax
    50004b0c:	48 89 db             	mov    %rbx,%rbx
    50004b0f:	0f 01 d7             	enclu  
    50004b12:	48 8b 45 f0          	mov    -0x10(%rbp),%rax
    50004b16:	48 8d 88 5c 04 00 00 	lea    0x45c(%rax),%rcx
    50004b1d:	48 8b 45 d8          	mov    -0x28(%rbp),%rax
    50004b21:	ba 10 00 00 00       	mov    $0x10,%edx
    50004b26:	48 89 ce             	mov    %rcx,%rsi
    50004b29:	48 89 c7             	mov    %rax,%rdi
    50004b2c:	e8 fd fa ff ff       	callq  5000462e <memcpy>
    50004b31:	48 8b 45 f0          	mov    -0x10(%rbp),%rax
    50004b35:	48 8d 88 5c 06 00 00 	lea    0x65c(%rax),%rcx
    50004b3c:	48 8b 45 d0          	mov    -0x30(%rbp),%rax
    50004b40:	ba 04 00 00 00       	mov    $0x4,%edx
    50004b45:	48 89 ce             	mov    %rcx,%rsi
    50004b48:	48 89 c7             	mov    %rax,%rdi
    50004b4b:	e8 de fa ff ff       	callq  5000462e <memcpy>
    50004b50:	48 8b 45 f0          	mov    -0x10(%rbp),%rax
    50004b54:	8b 80 30 04 00 00    	mov    0x430(%rax),%eax
    50004b5a:	48 83 c4 28          	add    $0x28,%rsp
    50004b5e:	5b                   	pop    %rbx
    50004b5f:	5d                   	pop    %rbp
    50004b60:	c3                   	retq   

0000000050004b61 <connect>:
    50004b61:	55                   	push   %rbp
    50004b62:	48 89 e5             	mov    %rsp,%rbp
    50004b65:	53                   	push   %rbx
    50004b66:	48 83 ec 20          	sub    $0x20,%rsp
    50004b6a:	89 7d e4             	mov    %edi,-0x1c(%rbp)
    50004b6d:	48 89 75 d8          	mov    %rsi,-0x28(%rbp)
    50004b71:	89 55 e0             	mov    %edx,-0x20(%rbp)
    50004b74:	b8 00 00 80 80       	mov    $0x80800000,%eax
    50004b79:	48 89 45 f0          	mov    %rax,-0x10(%rbp)
    50004b7d:	48 8b 45 f0          	mov    -0x10(%rbp),%rax
    50004b81:	c7 80 3c 04 00 00 0e 	movl   $0xe,0x43c(%rax)
    50004b88:	00 00 00 
    50004b8b:	48 8b 45 f0          	mov    -0x10(%rbp),%rax
    50004b8f:	8b 55 e4             	mov    -0x1c(%rbp),%edx
    50004b92:	89 90 50 04 00 00    	mov    %edx,0x450(%rax)
    50004b98:	8b 55 e0             	mov    -0x20(%rbp),%edx
    50004b9b:	48 8b 45 f0          	mov    -0x10(%rbp),%rax
    50004b9f:	48 8d 88 5c 04 00 00 	lea    0x45c(%rax),%rcx
    50004ba6:	48 8b 45 d8          	mov    -0x28(%rbp),%rax
    50004baa:	48 89 c6             	mov    %rax,%rsi
    50004bad:	48 89 cf             	mov    %rcx,%rdi
    50004bb0:	e8 79 fa ff ff       	callq  5000462e <memcpy>
    50004bb5:	8b 55 e0             	mov    -0x20(%rbp),%edx
    50004bb8:	48 8b 45 f0          	mov    -0x10(%rbp),%rax
    50004bbc:	89 90 54 04 00 00    	mov    %edx,0x454(%rax)
    50004bc2:	48 8b 45 f0          	mov    -0x10(%rbp),%rax
    50004bc6:	48 8b 40 08          	mov    0x8(%rax),%rax
    50004bca:	48 89 c2             	mov    %rax,%rdx
    50004bcd:	b8 04 00 00 00       	mov    $0x4,%eax
    50004bd2:	48 89 d3             	mov    %rdx,%rbx
    50004bd5:	89 c0                	mov    %eax,%eax
    50004bd7:	48 89 db             	mov    %rbx,%rbx
    50004bda:	0f 01 d7             	enclu  
    50004bdd:	48 8b 45 f0          	mov    -0x10(%rbp),%rax
    50004be1:	8b 80 30 04 00 00    	mov    0x430(%rax),%eax
    50004be7:	48 83 c4 20          	add    $0x20,%rsp
    50004beb:	5b                   	pop    %rbx
    50004bec:	5d                   	pop    %rbp
    50004bed:	c3                   	retq   

0000000050004bee <send>:
    50004bee:	55                   	push   %rbp
    50004bef:	48 89 e5             	mov    %rsp,%rbp
    50004bf2:	53                   	push   %rbx
    50004bf3:	48 83 ec 28          	sub    $0x28,%rsp
    50004bf7:	89 7d e4             	mov    %edi,-0x1c(%rbp)
    50004bfa:	48 89 75 d8          	mov    %rsi,-0x28(%rbp)
    50004bfe:	48 89 55 d0          	mov    %rdx,-0x30(%rbp)
    50004c02:	89 4d e0             	mov    %ecx,-0x20(%rbp)
    50004c05:	b8 00 00 80 80       	mov    $0x80800000,%eax
    50004c0a:	48 89 45 f0          	mov    %rax,-0x10(%rbp)
    50004c0e:	48 8b 45 f0          	mov    -0x10(%rbp),%rax
    50004c12:	c7 80 3c 04 00 00 0f 	movl   $0xf,0x43c(%rax)
    50004c19:	00 00 00 
    50004c1c:	48 8b 45 f0          	mov    -0x10(%rbp),%rax
    50004c20:	48 8d 88 5c 04 00 00 	lea    0x45c(%rax),%rcx
    50004c27:	48 8b 55 d0          	mov    -0x30(%rbp),%rdx
    50004c2b:	48 8b 45 d8          	mov    -0x28(%rbp),%rax
    50004c2f:	48 89 c6             	mov    %rax,%rsi
    50004c32:	48 89 cf             	mov    %rcx,%rdi
    50004c35:	e8 f4 f9 ff ff       	callq  5000462e <memcpy>
    50004c3a:	48 8b 45 f0          	mov    -0x10(%rbp),%rax
    50004c3e:	8b 55 e4             	mov    -0x1c(%rbp),%edx
    50004c41:	89 90 50 04 00 00    	mov    %edx,0x450(%rax)
    50004c47:	48 8b 45 d0          	mov    -0x30(%rbp),%rax
    50004c4b:	89 c2                	mov    %eax,%edx
    50004c4d:	48 8b 45 f0          	mov    -0x10(%rbp),%rax
    50004c51:	89 90 54 04 00 00    	mov    %edx,0x454(%rax)
    50004c57:	48 8b 45 f0          	mov    -0x10(%rbp),%rax
    50004c5b:	8b 55 e0             	mov    -0x20(%rbp),%edx
    50004c5e:	89 90 58 04 00 00    	mov    %edx,0x458(%rax)
    50004c64:	48 8b 45 f0          	mov    -0x10(%rbp),%rax
    50004c68:	48 8b 40 08          	mov    0x8(%rax),%rax
    50004c6c:	48 89 c2             	mov    %rax,%rdx
    50004c6f:	b8 04 00 00 00       	mov    $0x4,%eax
    50004c74:	48 89 d3             	mov    %rdx,%rbx
    50004c77:	89 c0                	mov    %eax,%eax
    50004c79:	48 89 db             	mov    %rbx,%rbx
    50004c7c:	0f 01 d7             	enclu  
    50004c7f:	48 8b 45 f0          	mov    -0x10(%rbp),%rax
    50004c83:	8b 80 30 04 00 00    	mov    0x430(%rax),%eax
    50004c89:	48 98                	cltq   
    50004c8b:	48 83 c4 28          	add    $0x28,%rsp
    50004c8f:	5b                   	pop    %rbx
    50004c90:	5d                   	pop    %rbp
    50004c91:	c3                   	retq   

0000000050004c92 <recv>:
    50004c92:	55                   	push   %rbp
    50004c93:	48 89 e5             	mov    %rsp,%rbp
    50004c96:	53                   	push   %rbx
    50004c97:	48 83 ec 28          	sub    $0x28,%rsp
    50004c9b:	89 7d e4             	mov    %edi,-0x1c(%rbp)
    50004c9e:	48 89 75 d8          	mov    %rsi,-0x28(%rbp)
    50004ca2:	48 89 55 d0          	mov    %rdx,-0x30(%rbp)
    50004ca6:	89 4d e0             	mov    %ecx,-0x20(%rbp)
    50004ca9:	b8 00 00 80 80       	mov    $0x80800000,%eax
    50004cae:	48 89 45 f0          	mov    %rax,-0x10(%rbp)
    50004cb2:	48 8b 45 f0          	mov    -0x10(%rbp),%rax
    50004cb6:	c7 80 3c 04 00 00 10 	movl   $0x10,0x43c(%rax)
    50004cbd:	00 00 00 
    50004cc0:	48 8b 45 f0          	mov    -0x10(%rbp),%rax
    50004cc4:	8b 55 e4             	mov    -0x1c(%rbp),%edx
    50004cc7:	89 90 50 04 00 00    	mov    %edx,0x450(%rax)
    50004ccd:	48 8b 45 d0          	mov    -0x30(%rbp),%rax
    50004cd1:	89 c2                	mov    %eax,%edx
    50004cd3:	48 8b 45 f0          	mov    -0x10(%rbp),%rax
    50004cd7:	89 90 54 04 00 00    	mov    %edx,0x454(%rax)
    50004cdd:	48 8b 45 f0          	mov    -0x10(%rbp),%rax
    50004ce1:	8b 55 e0             	mov    -0x20(%rbp),%edx
    50004ce4:	89 90 58 04 00 00    	mov    %edx,0x458(%rax)
    50004cea:	48 8b 45 f0          	mov    -0x10(%rbp),%rax
    50004cee:	48 8b 40 08          	mov    0x8(%rax),%rax
    50004cf2:	48 89 c2             	mov    %rax,%rdx
    50004cf5:	b8 04 00 00 00       	mov    $0x4,%eax
    50004cfa:	48 89 d3             	mov    %rdx,%rbx
    50004cfd:	89 c0                	mov    %eax,%eax
    50004cff:	48 89 db             	mov    %rbx,%rbx
    50004d02:	0f 01 d7             	enclu  
    50004d05:	48 8b 45 f0          	mov    -0x10(%rbp),%rax
    50004d09:	48 8d 48 30          	lea    0x30(%rax),%rcx
    50004d0d:	48 8b 55 d0          	mov    -0x30(%rbp),%rdx
    50004d11:	48 8b 45 d8          	mov    -0x28(%rbp),%rax
    50004d15:	48 89 ce             	mov    %rcx,%rsi
    50004d18:	48 89 c7             	mov    %rax,%rdi
    50004d1b:	e8 0e f9 ff ff       	callq  5000462e <memcpy>
    50004d20:	48 8b 45 f0          	mov    -0x10(%rbp),%rax
    50004d24:	8b 80 30 04 00 00    	mov    0x430(%rax),%eax
    50004d2a:	48 98                	cltq   
    50004d2c:	48 83 c4 28          	add    $0x28,%rsp
    50004d30:	5b                   	pop    %rbx
    50004d31:	5d                   	pop    %rbp
    50004d32:	c3                   	retq   

0000000050004d33 <sgx_enclave_read>:
    50004d33:	55                   	push   %rbp
    50004d34:	48 89 e5             	mov    %rsp,%rbp
    50004d37:	48 83 ec 20          	sub    $0x20,%rsp
    50004d3b:	48 89 7d e8          	mov    %rdi,-0x18(%rbp)
    50004d3f:	89 75 e4             	mov    %esi,-0x1c(%rbp)
    50004d42:	b8 00 00 80 80       	mov    $0x80800000,%eax
    50004d47:	48 89 45 f8          	mov    %rax,-0x8(%rbp)
    50004d4b:	83 7d e4 00          	cmpl   $0x0,-0x1c(%rbp)
    50004d4f:	7f 07                	jg     50004d58 <sgx_enclave_read+0x25>
    50004d51:	b8 ff ff ff ff       	mov    $0xffffffff,%eax
    50004d56:	eb 3a                	jmp    50004d92 <sgx_enclave_read+0x5f>
    50004d58:	8b 45 e4             	mov    -0x1c(%rbp),%eax
    50004d5b:	48 63 d0             	movslq %eax,%rdx
    50004d5e:	48 8b 45 f8          	mov    -0x8(%rbp),%rax
    50004d62:	48 8d 48 30          	lea    0x30(%rax),%rcx
    50004d66:	48 8b 45 e8          	mov    -0x18(%rbp),%rax
    50004d6a:	48 89 ce             	mov    %rcx,%rsi
    50004d6d:	48 89 c7             	mov    %rax,%rdi
    50004d70:	e8 b9 f8 ff ff       	callq  5000462e <memcpy>
    50004d75:	48 8b 45 f8          	mov    -0x8(%rbp),%rax
    50004d79:	48 83 c0 30          	add    $0x30,%rax
    50004d7d:	ba 00 02 00 00       	mov    $0x200,%edx
    50004d82:	be 00 00 00 00       	mov    $0x0,%esi
    50004d87:	48 89 c7             	mov    %rax,%rdi
    50004d8a:	e8 28 09 00 00       	callq  500056b7 <memset>
    50004d8f:	8b 45 e4             	mov    -0x1c(%rbp),%eax
    50004d92:	c9                   	leaveq 
    50004d93:	c3                   	retq   

0000000050004d94 <sgx_enclave_write>:
    50004d94:	55                   	push   %rbp
    50004d95:	48 89 e5             	mov    %rsp,%rbp
    50004d98:	48 83 ec 20          	sub    $0x20,%rsp
    50004d9c:	48 89 7d e8          	mov    %rdi,-0x18(%rbp)
    50004da0:	89 75 e4             	mov    %esi,-0x1c(%rbp)
    50004da3:	b8 00 00 80 80       	mov    $0x80800000,%eax
    50004da8:	48 89 45 f8          	mov    %rax,-0x8(%rbp)
    50004dac:	83 7d e4 00          	cmpl   $0x0,-0x1c(%rbp)
    50004db0:	7f 07                	jg     50004db9 <sgx_enclave_write+0x25>
    50004db2:	b8 ff ff ff ff       	mov    $0xffffffff,%eax
    50004db7:	eb 3f                	jmp    50004df8 <sgx_enclave_write+0x64>
    50004db9:	48 8b 45 f8          	mov    -0x8(%rbp),%rax
    50004dbd:	48 05 5c 04 00 00    	add    $0x45c,%rax
    50004dc3:	ba 00 02 00 00       	mov    $0x200,%edx
    50004dc8:	be 00 00 00 00       	mov    $0x0,%esi
    50004dcd:	48 89 c7             	mov    %rax,%rdi
    50004dd0:	e8 e2 08 00 00       	callq  500056b7 <memset>
    50004dd5:	8b 45 e4             	mov    -0x1c(%rbp),%eax
    50004dd8:	48 63 d0             	movslq %eax,%rdx
    50004ddb:	48 8b 45 f8          	mov    -0x8(%rbp),%rax
    50004ddf:	48 8d 88 5c 04 00 00 	lea    0x45c(%rax),%rcx
    50004de6:	48 8b 45 e8          	mov    -0x18(%rbp),%rax
    50004dea:	48 89 c6             	mov    %rax,%rsi
    50004ded:	48 89 cf             	mov    %rcx,%rdi
    50004df0:	e8 39 f8 ff ff       	callq  5000462e <memcpy>
    50004df5:	8b 45 e4             	mov    -0x1c(%rbp),%eax
    50004df8:	c9                   	leaveq 
    50004df9:	c3                   	retq   

0000000050004dfa <sgx_putchar>:
    50004dfa:	55                   	push   %rbp
    50004dfb:	48 89 e5             	mov    %rsp,%rbp
    50004dfe:	53                   	push   %rbx
    50004dff:	89 f8                	mov    %edi,%eax
    50004e01:	88 45 e4             	mov    %al,-0x1c(%rbp)
    50004e04:	b8 00 00 80 80       	mov    $0x80800000,%eax
    50004e09:	48 89 45 f0          	mov    %rax,-0x10(%rbp)
    50004e0d:	0f be 55 e4          	movsbl -0x1c(%rbp),%edx
    50004e11:	48 8b 45 f0          	mov    -0x10(%rbp),%rax
    50004e15:	89 90 50 04 00 00    	mov    %edx,0x450(%rax)
    50004e1b:	48 8b 45 f0          	mov    -0x10(%rbp),%rax
    50004e1f:	c7 80 3c 04 00 00 02 	movl   $0x2,0x43c(%rax)
    50004e26:	00 00 00 
    50004e29:	48 8b 45 f0          	mov    -0x10(%rbp),%rax
    50004e2d:	48 8b 40 08          	mov    0x8(%rax),%rax
    50004e31:	48 89 c2             	mov    %rax,%rdx
    50004e34:	b8 04 00 00 00       	mov    $0x4,%eax
    50004e39:	48 89 d3             	mov    %rdx,%rbx
    50004e3c:	89 c0                	mov    %eax,%eax
    50004e3e:	48 89 db             	mov    %rbx,%rbx
    50004e41:	0f 01 d7             	enclu  
    50004e44:	5b                   	pop    %rbx
    50004e45:	5d                   	pop    %rbp
    50004e46:	c3                   	retq   

0000000050004e47 <printchar>:
    50004e47:	55                   	push   %rbp
    50004e48:	48 89 e5             	mov    %rsp,%rbp
    50004e4b:	48 83 ec 10          	sub    $0x10,%rsp
    50004e4f:	48 89 7d f8          	mov    %rdi,-0x8(%rbp)
    50004e53:	89 75 f4             	mov    %esi,-0xc(%rbp)
    50004e56:	8b 45 f4             	mov    -0xc(%rbp),%eax
    50004e59:	0f be c0             	movsbl %al,%eax
    50004e5c:	89 c7                	mov    %eax,%edi
    50004e5e:	e8 97 ff ff ff       	callq  50004dfa <sgx_putchar>
    50004e63:	c9                   	leaveq 
    50004e64:	c3                   	retq   

0000000050004e65 <prints>:
    50004e65:	55                   	push   %rbp
    50004e66:	48 89 e5             	mov    %rsp,%rbp
    50004e69:	41 56                	push   %r14
    50004e6b:	41 55                	push   %r13
    50004e6d:	41 54                	push   %r12
    50004e6f:	53                   	push   %rbx
    50004e70:	48 83 ec 18          	sub    $0x18,%rsp
    50004e74:	48 89 7d d8          	mov    %rdi,-0x28(%rbp)
    50004e78:	48 89 75 d0          	mov    %rsi,-0x30(%rbp)
    50004e7c:	89 55 cc             	mov    %edx,-0x34(%rbp)
    50004e7f:	89 4d c8             	mov    %ecx,-0x38(%rbp)
    50004e82:	bb 00 00 00 00       	mov    $0x0,%ebx
    50004e87:	41 be 20 00 00 00    	mov    $0x20,%r14d
    50004e8d:	83 7d cc 00          	cmpl   $0x0,-0x34(%rbp)
    50004e91:	7e 40                	jle    50004ed3 <prints+0x6e>
    50004e93:	41 bc 00 00 00 00    	mov    $0x0,%r12d
    50004e99:	4c 8b 6d d0          	mov    -0x30(%rbp),%r13
    50004e9d:	eb 08                	jmp    50004ea7 <prints+0x42>
    50004e9f:	41 83 c4 01          	add    $0x1,%r12d
    50004ea3:	49 83 c5 01          	add    $0x1,%r13
    50004ea7:	41 0f b6 45 00       	movzbl 0x0(%r13),%eax
    50004eac:	84 c0                	test   %al,%al
    50004eae:	75 ef                	jne    50004e9f <prints+0x3a>
    50004eb0:	44 3b 65 cc          	cmp    -0x34(%rbp),%r12d
    50004eb4:	7c 09                	jl     50004ebf <prints+0x5a>
    50004eb6:	c7 45 cc 00 00 00 00 	movl   $0x0,-0x34(%rbp)
    50004ebd:	eb 04                	jmp    50004ec3 <prints+0x5e>
    50004ebf:	44 29 65 cc          	sub    %r12d,-0x34(%rbp)
    50004ec3:	8b 45 c8             	mov    -0x38(%rbp),%eax
    50004ec6:	83 e0 02             	and    $0x2,%eax
    50004ec9:	85 c0                	test   %eax,%eax
    50004ecb:	74 06                	je     50004ed3 <prints+0x6e>
    50004ecd:	41 be 30 00 00 00    	mov    $0x30,%r14d
    50004ed3:	8b 45 c8             	mov    -0x38(%rbp),%eax
    50004ed6:	83 e0 01             	and    $0x1,%eax
    50004ed9:	85 c0                	test   %eax,%eax
    50004edb:	75 1e                	jne    50004efb <prints+0x96>
    50004edd:	eb 16                	jmp    50004ef5 <prints+0x90>
    50004edf:	48 8b 45 d8          	mov    -0x28(%rbp),%rax
    50004ee3:	44 89 f6             	mov    %r14d,%esi
    50004ee6:	48 89 c7             	mov    %rax,%rdi
    50004ee9:	e8 59 ff ff ff       	callq  50004e47 <printchar>
    50004eee:	83 c3 01             	add    $0x1,%ebx
    50004ef1:	83 6d cc 01          	subl   $0x1,-0x34(%rbp)
    50004ef5:	83 7d cc 00          	cmpl   $0x0,-0x34(%rbp)
    50004ef9:	7f e4                	jg     50004edf <prints+0x7a>
    50004efb:	eb 20                	jmp    50004f1d <prints+0xb8>
    50004efd:	48 8b 45 d0          	mov    -0x30(%rbp),%rax
    50004f01:	0f b6 00             	movzbl (%rax),%eax
    50004f04:	0f be d0             	movsbl %al,%edx
    50004f07:	48 8b 45 d8          	mov    -0x28(%rbp),%rax
    50004f0b:	89 d6                	mov    %edx,%esi
    50004f0d:	48 89 c7             	mov    %rax,%rdi
    50004f10:	e8 32 ff ff ff       	callq  50004e47 <printchar>
    50004f15:	83 c3 01             	add    $0x1,%ebx
    50004f18:	48 83 45 d0 01       	addq   $0x1,-0x30(%rbp)
    50004f1d:	48 8b 45 d0          	mov    -0x30(%rbp),%rax
    50004f21:	0f b6 00             	movzbl (%rax),%eax
    50004f24:	84 c0                	test   %al,%al
    50004f26:	75 d5                	jne    50004efd <prints+0x98>
    50004f28:	eb 16                	jmp    50004f40 <prints+0xdb>
    50004f2a:	48 8b 45 d8          	mov    -0x28(%rbp),%rax
    50004f2e:	44 89 f6             	mov    %r14d,%esi
    50004f31:	48 89 c7             	mov    %rax,%rdi
    50004f34:	e8 0e ff ff ff       	callq  50004e47 <printchar>
    50004f39:	83 c3 01             	add    $0x1,%ebx
    50004f3c:	83 6d cc 01          	subl   $0x1,-0x34(%rbp)
    50004f40:	83 7d cc 00          	cmpl   $0x0,-0x34(%rbp)
    50004f44:	7f e4                	jg     50004f2a <prints+0xc5>
    50004f46:	89 d8                	mov    %ebx,%eax
    50004f48:	48 83 c4 18          	add    $0x18,%rsp
    50004f4c:	5b                   	pop    %rbx
    50004f4d:	41 5c                	pop    %r12
    50004f4f:	41 5d                	pop    %r13
    50004f51:	41 5e                	pop    %r14
    50004f53:	5d                   	pop    %rbp
    50004f54:	c3                   	retq   

0000000050004f55 <printi>:
    50004f55:	55                   	push   %rbp
    50004f56:	48 89 e5             	mov    %rsp,%rbp
    50004f59:	41 57                	push   %r15
    50004f5b:	41 56                	push   %r14
    50004f5d:	41 55                	push   %r13
    50004f5f:	41 54                	push   %r12
    50004f61:	53                   	push   %rbx
    50004f62:	48 83 ec 30          	sub    $0x30,%rsp
    50004f66:	48 89 7d c0          	mov    %rdi,-0x40(%rbp)
    50004f6a:	89 75 bc             	mov    %esi,-0x44(%rbp)
    50004f6d:	89 55 b8             	mov    %edx,-0x48(%rbp)
    50004f70:	89 4d b4             	mov    %ecx,-0x4c(%rbp)
    50004f73:	44 89 45 b0          	mov    %r8d,-0x50(%rbp)
    50004f77:	44 89 4d ac          	mov    %r9d,-0x54(%rbp)
    50004f7b:	41 bf 00 00 00 00    	mov    $0x0,%r15d
    50004f81:	41 be 00 00 00 00    	mov    $0x0,%r14d
    50004f87:	44 8b 65 bc          	mov    -0x44(%rbp),%r12d
    50004f8b:	83 7d bc 00          	cmpl   $0x0,-0x44(%rbp)
    50004f8f:	75 23                	jne    50004fb4 <printi+0x5f>
    50004f91:	c6 45 c8 30          	movb   $0x30,-0x38(%rbp)
    50004f95:	c6 45 c9 00          	movb   $0x0,-0x37(%rbp)
    50004f99:	8b 4d ac             	mov    -0x54(%rbp),%ecx
    50004f9c:	8b 55 b0             	mov    -0x50(%rbp),%edx
    50004f9f:	48 8d 75 c8          	lea    -0x38(%rbp),%rsi
    50004fa3:	48 8b 45 c0          	mov    -0x40(%rbp),%rax
    50004fa7:	48 89 c7             	mov    %rax,%rdi
    50004faa:	e8 b6 fe ff ff       	callq  50004e65 <prints>
    50004faf:	e9 be 00 00 00       	jmpq   50005072 <printi+0x11d>
    50004fb4:	83 7d b4 00          	cmpl   $0x0,-0x4c(%rbp)
    50004fb8:	74 1a                	je     50004fd4 <printi+0x7f>
    50004fba:	83 7d b8 0a          	cmpl   $0xa,-0x48(%rbp)
    50004fbe:	75 14                	jne    50004fd4 <printi+0x7f>
    50004fc0:	83 7d bc 00          	cmpl   $0x0,-0x44(%rbp)
    50004fc4:	79 0e                	jns    50004fd4 <printi+0x7f>
    50004fc6:	41 bf 01 00 00 00    	mov    $0x1,%r15d
    50004fcc:	8b 45 bc             	mov    -0x44(%rbp),%eax
    50004fcf:	f7 d8                	neg    %eax
    50004fd1:	41 89 c4             	mov    %eax,%r12d
    50004fd4:	48 8d 5d c8          	lea    -0x38(%rbp),%rbx
    50004fd8:	48 83 c3 0b          	add    $0xb,%rbx
    50004fdc:	c6 03 00             	movb   $0x0,(%rbx)
    50004fdf:	eb 3d                	jmp    5000501e <printi+0xc9>
    50004fe1:	8b 4d b8             	mov    -0x48(%rbp),%ecx
    50004fe4:	44 89 e0             	mov    %r12d,%eax
    50004fe7:	ba 00 00 00 00       	mov    $0x0,%edx
    50004fec:	f7 f1                	div    %ecx
    50004fee:	89 d0                	mov    %edx,%eax
    50004ff0:	41 89 c5             	mov    %eax,%r13d
    50004ff3:	41 83 fd 09          	cmp    $0x9,%r13d
    50004ff7:	7e 09                	jle    50005002 <printi+0xad>
    50004ff9:	8b 45 10             	mov    0x10(%rbp),%eax
    50004ffc:	83 e8 3a             	sub    $0x3a,%eax
    50004fff:	41 01 c5             	add    %eax,%r13d
    50005002:	48 83 eb 01          	sub    $0x1,%rbx
    50005006:	44 89 e8             	mov    %r13d,%eax
    50005009:	83 c0 30             	add    $0x30,%eax
    5000500c:	88 03                	mov    %al,(%rbx)
    5000500e:	8b 7d b8             	mov    -0x48(%rbp),%edi
    50005011:	44 89 e0             	mov    %r12d,%eax
    50005014:	ba 00 00 00 00       	mov    $0x0,%edx
    50005019:	f7 f7                	div    %edi
    5000501b:	41 89 c4             	mov    %eax,%r12d
    5000501e:	45 85 e4             	test   %r12d,%r12d
    50005021:	75 be                	jne    50004fe1 <printi+0x8c>
    50005023:	45 85 ff             	test   %r15d,%r15d
    50005026:	74 32                	je     5000505a <printi+0x105>
    50005028:	83 7d b0 00          	cmpl   $0x0,-0x50(%rbp)
    5000502c:	74 25                	je     50005053 <printi+0xfe>
    5000502e:	8b 45 ac             	mov    -0x54(%rbp),%eax
    50005031:	83 e0 02             	and    $0x2,%eax
    50005034:	85 c0                	test   %eax,%eax
    50005036:	74 1b                	je     50005053 <printi+0xfe>
    50005038:	48 8b 45 c0          	mov    -0x40(%rbp),%rax
    5000503c:	be 2d 00 00 00       	mov    $0x2d,%esi
    50005041:	48 89 c7             	mov    %rax,%rdi
    50005044:	e8 fe fd ff ff       	callq  50004e47 <printchar>
    50005049:	41 83 c6 01          	add    $0x1,%r14d
    5000504d:	83 6d b0 01          	subl   $0x1,-0x50(%rbp)
    50005051:	eb 07                	jmp    5000505a <printi+0x105>
    50005053:	48 83 eb 01          	sub    $0x1,%rbx
    50005057:	c6 03 2d             	movb   $0x2d,(%rbx)
    5000505a:	8b 4d ac             	mov    -0x54(%rbp),%ecx
    5000505d:	8b 55 b0             	mov    -0x50(%rbp),%edx
    50005060:	48 8b 45 c0          	mov    -0x40(%rbp),%rax
    50005064:	48 89 de             	mov    %rbx,%rsi
    50005067:	48 89 c7             	mov    %rax,%rdi
    5000506a:	e8 f6 fd ff ff       	callq  50004e65 <prints>
    5000506f:	44 01 f0             	add    %r14d,%eax
    50005072:	48 83 c4 30          	add    $0x30,%rsp
    50005076:	5b                   	pop    %rbx
    50005077:	41 5c                	pop    %r12
    50005079:	41 5d                	pop    %r13
    5000507b:	41 5e                	pop    %r14
    5000507d:	41 5f                	pop    %r15
    5000507f:	5d                   	pop    %rbp
    50005080:	c3                   	retq   

0000000050005081 <sgx_print>:
    50005081:	55                   	push   %rbp
    50005082:	48 89 e5             	mov    %rsp,%rbp
    50005085:	41 56                	push   %r14
    50005087:	41 55                	push   %r13
    50005089:	41 54                	push   %r12
    5000508b:	53                   	push   %rbx
    5000508c:	48 83 ec 28          	sub    $0x28,%rsp
    50005090:	48 89 7d c8          	mov    %rdi,-0x38(%rbp)
    50005094:	48 89 75 c0          	mov    %rsi,-0x40(%rbp)
    50005098:	48 89 55 b8          	mov    %rdx,-0x48(%rbp)
    5000509c:	bb 00 00 00 00       	mov    $0x0,%ebx
    500050a1:	e9 9c 03 00 00       	jmpq   50005442 <sgx_print+0x3c1>
    500050a6:	48 8b 45 c0          	mov    -0x40(%rbp),%rax
    500050aa:	0f b6 00             	movzbl (%rax),%eax
    500050ad:	3c 25                	cmp    $0x25,%al
    500050af:	0f 85 6d 03 00 00    	jne    50005422 <sgx_print+0x3a1>
    500050b5:	48 83 45 c0 01       	addq   $0x1,-0x40(%rbp)
    500050ba:	41 bd 00 00 00 00    	mov    $0x0,%r13d
    500050c0:	45 89 ec             	mov    %r13d,%r12d
    500050c3:	48 8b 45 c0          	mov    -0x40(%rbp),%rax
    500050c7:	0f b6 00             	movzbl (%rax),%eax
    500050ca:	84 c0                	test   %al,%al
    500050cc:	75 05                	jne    500050d3 <sgx_print+0x52>
    500050ce:	e9 7e 03 00 00       	jmpq   50005451 <sgx_print+0x3d0>
    500050d3:	48 8b 45 c0          	mov    -0x40(%rbp),%rax
    500050d7:	0f b6 00             	movzbl (%rax),%eax
    500050da:	3c 25                	cmp    $0x25,%al
    500050dc:	75 05                	jne    500050e3 <sgx_print+0x62>
    500050de:	e9 3f 03 00 00       	jmpq   50005422 <sgx_print+0x3a1>
    500050e3:	48 8b 45 c0          	mov    -0x40(%rbp),%rax
    500050e7:	0f b6 00             	movzbl (%rax),%eax
    500050ea:	3c 2d                	cmp    $0x2d,%al
    500050ec:	75 0b                	jne    500050f9 <sgx_print+0x78>
    500050ee:	48 83 45 c0 01       	addq   $0x1,-0x40(%rbp)
    500050f3:	41 bd 01 00 00 00    	mov    $0x1,%r13d
    500050f9:	eb 09                	jmp    50005104 <sgx_print+0x83>
    500050fb:	48 83 45 c0 01       	addq   $0x1,-0x40(%rbp)
    50005100:	41 83 cd 02          	or     $0x2,%r13d
    50005104:	48 8b 45 c0          	mov    -0x40(%rbp),%rax
    50005108:	0f b6 00             	movzbl (%rax),%eax
    5000510b:	3c 30                	cmp    $0x30,%al
    5000510d:	74 ec                	je     500050fb <sgx_print+0x7a>
    5000510f:	eb 23                	jmp    50005134 <sgx_print+0xb3>
    50005111:	44 89 e0             	mov    %r12d,%eax
    50005114:	c1 e0 02             	shl    $0x2,%eax
    50005117:	44 01 e0             	add    %r12d,%eax
    5000511a:	01 c0                	add    %eax,%eax
    5000511c:	41 89 c4             	mov    %eax,%r12d
    5000511f:	48 8b 45 c0          	mov    -0x40(%rbp),%rax
    50005123:	0f b6 00             	movzbl (%rax),%eax
    50005126:	0f be c0             	movsbl %al,%eax
    50005129:	83 e8 30             	sub    $0x30,%eax
    5000512c:	41 01 c4             	add    %eax,%r12d
    5000512f:	48 83 45 c0 01       	addq   $0x1,-0x40(%rbp)
    50005134:	48 8b 45 c0          	mov    -0x40(%rbp),%rax
    50005138:	0f b6 00             	movzbl (%rax),%eax
    5000513b:	3c 2f                	cmp    $0x2f,%al
    5000513d:	7e 0b                	jle    5000514a <sgx_print+0xc9>
    5000513f:	48 8b 45 c0          	mov    -0x40(%rbp),%rax
    50005143:	0f b6 00             	movzbl (%rax),%eax
    50005146:	3c 39                	cmp    $0x39,%al
    50005148:	7e c7                	jle    50005111 <sgx_print+0x90>
    5000514a:	48 8b 45 c0          	mov    -0x40(%rbp),%rax
    5000514e:	0f b6 00             	movzbl (%rax),%eax
    50005151:	3c 73                	cmp    $0x73,%al
    50005153:	75 74                	jne    500051c9 <sgx_print+0x148>
    50005155:	48 8b 45 b8          	mov    -0x48(%rbp),%rax
    50005159:	8b 00                	mov    (%rax),%eax
    5000515b:	83 f8 30             	cmp    $0x30,%eax
    5000515e:	73 24                	jae    50005184 <sgx_print+0x103>
    50005160:	48 8b 45 b8          	mov    -0x48(%rbp),%rax
    50005164:	48 8b 50 10          	mov    0x10(%rax),%rdx
    50005168:	48 8b 45 b8          	mov    -0x48(%rbp),%rax
    5000516c:	8b 00                	mov    (%rax),%eax
    5000516e:	89 c0                	mov    %eax,%eax
    50005170:	48 01 d0             	add    %rdx,%rax
    50005173:	48 8b 55 b8          	mov    -0x48(%rbp),%rdx
    50005177:	8b 12                	mov    (%rdx),%edx
    50005179:	8d 4a 08             	lea    0x8(%rdx),%ecx
    5000517c:	48 8b 55 b8          	mov    -0x48(%rbp),%rdx
    50005180:	89 0a                	mov    %ecx,(%rdx)
    50005182:	eb 14                	jmp    50005198 <sgx_print+0x117>
    50005184:	48 8b 45 b8          	mov    -0x48(%rbp),%rax
    50005188:	48 8b 40 08          	mov    0x8(%rax),%rax
    5000518c:	48 8d 48 08          	lea    0x8(%rax),%rcx
    50005190:	48 8b 55 b8          	mov    -0x48(%rbp),%rdx
    50005194:	48 89 4a 08          	mov    %rcx,0x8(%rdx)
    50005198:	8b 00                	mov    (%rax),%eax
    5000519a:	48 98                	cltq   
    5000519c:	49 89 c6             	mov    %rax,%r14
    5000519f:	4d 85 f6             	test   %r14,%r14
    500051a2:	74 05                	je     500051a9 <sgx_print+0x128>
    500051a4:	4c 89 f0             	mov    %r14,%rax
    500051a7:	eb 07                	jmp    500051b0 <sgx_print+0x12f>
    500051a9:	48 8d 05 6a 08 00 00 	lea    0x86a(%rip),%rax        # 50005a1a <strlen+0x1df>
    500051b0:	48 8b 7d c8          	mov    -0x38(%rbp),%rdi
    500051b4:	44 89 e9             	mov    %r13d,%ecx
    500051b7:	44 89 e2             	mov    %r12d,%edx
    500051ba:	48 89 c6             	mov    %rax,%rsi
    500051bd:	e8 a3 fc ff ff       	callq  50004e65 <prints>
    500051c2:	01 c3                	add    %eax,%ebx
    500051c4:	e9 74 02 00 00       	jmpq   5000543d <sgx_print+0x3bc>
    500051c9:	48 8b 45 c0          	mov    -0x40(%rbp),%rax
    500051cd:	0f b6 00             	movzbl (%rax),%eax
    500051d0:	3c 64                	cmp    $0x64,%al
    500051d2:	75 6e                	jne    50005242 <sgx_print+0x1c1>
    500051d4:	48 8b 45 b8          	mov    -0x48(%rbp),%rax
    500051d8:	8b 00                	mov    (%rax),%eax
    500051da:	83 f8 30             	cmp    $0x30,%eax
    500051dd:	73 24                	jae    50005203 <sgx_print+0x182>
    500051df:	48 8b 45 b8          	mov    -0x48(%rbp),%rax
    500051e3:	48 8b 50 10          	mov    0x10(%rax),%rdx
    500051e7:	48 8b 45 b8          	mov    -0x48(%rbp),%rax
    500051eb:	8b 00                	mov    (%rax),%eax
    500051ed:	89 c0                	mov    %eax,%eax
    500051ef:	48 01 d0             	add    %rdx,%rax
    500051f2:	48 8b 55 b8          	mov    -0x48(%rbp),%rdx
    500051f6:	8b 12                	mov    (%rdx),%edx
    500051f8:	8d 4a 08             	lea    0x8(%rdx),%ecx
    500051fb:	48 8b 55 b8          	mov    -0x48(%rbp),%rdx
    500051ff:	89 0a                	mov    %ecx,(%rdx)
    50005201:	eb 14                	jmp    50005217 <sgx_print+0x196>
    50005203:	48 8b 45 b8          	mov    -0x48(%rbp),%rax
    50005207:	48 8b 40 08          	mov    0x8(%rax),%rax
    5000520b:	48 8d 48 08          	lea    0x8(%rax),%rcx
    5000520f:	48 8b 55 b8          	mov    -0x48(%rbp),%rdx
    50005213:	48 89 4a 08          	mov    %rcx,0x8(%rdx)
    50005217:	8b 30                	mov    (%rax),%esi
    50005219:	48 8b 45 c8          	mov    -0x38(%rbp),%rax
    5000521d:	6a 61                	pushq  $0x61
    5000521f:	45 89 e9             	mov    %r13d,%r9d
    50005222:	45 89 e0             	mov    %r12d,%r8d
    50005225:	b9 01 00 00 00       	mov    $0x1,%ecx
    5000522a:	ba 0a 00 00 00       	mov    $0xa,%edx
    5000522f:	48 89 c7             	mov    %rax,%rdi
    50005232:	e8 1e fd ff ff       	callq  50004f55 <printi>
    50005237:	48 83 c4 08          	add    $0x8,%rsp
    5000523b:	01 c3                	add    %eax,%ebx
    5000523d:	e9 fb 01 00 00       	jmpq   5000543d <sgx_print+0x3bc>
    50005242:	48 8b 45 c0          	mov    -0x40(%rbp),%rax
    50005246:	0f b6 00             	movzbl (%rax),%eax
    50005249:	3c 78                	cmp    $0x78,%al
    5000524b:	75 6e                	jne    500052bb <sgx_print+0x23a>
    5000524d:	48 8b 45 b8          	mov    -0x48(%rbp),%rax
    50005251:	8b 00                	mov    (%rax),%eax
    50005253:	83 f8 30             	cmp    $0x30,%eax
    50005256:	73 24                	jae    5000527c <sgx_print+0x1fb>
    50005258:	48 8b 45 b8          	mov    -0x48(%rbp),%rax
    5000525c:	48 8b 50 10          	mov    0x10(%rax),%rdx
    50005260:	48 8b 45 b8          	mov    -0x48(%rbp),%rax
    50005264:	8b 00                	mov    (%rax),%eax
    50005266:	89 c0                	mov    %eax,%eax
    50005268:	48 01 d0             	add    %rdx,%rax
    5000526b:	48 8b 55 b8          	mov    -0x48(%rbp),%rdx
    5000526f:	8b 12                	mov    (%rdx),%edx
    50005271:	8d 4a 08             	lea    0x8(%rdx),%ecx
    50005274:	48 8b 55 b8          	mov    -0x48(%rbp),%rdx
    50005278:	89 0a                	mov    %ecx,(%rdx)
    5000527a:	eb 14                	jmp    50005290 <sgx_print+0x20f>
    5000527c:	48 8b 45 b8          	mov    -0x48(%rbp),%rax
    50005280:	48 8b 40 08          	mov    0x8(%rax),%rax
    50005284:	48 8d 48 08          	lea    0x8(%rax),%rcx
    50005288:	48 8b 55 b8          	mov    -0x48(%rbp),%rdx
    5000528c:	48 89 4a 08          	mov    %rcx,0x8(%rdx)
    50005290:	8b 30                	mov    (%rax),%esi
    50005292:	48 8b 45 c8          	mov    -0x38(%rbp),%rax
    50005296:	6a 61                	pushq  $0x61
    50005298:	45 89 e9             	mov    %r13d,%r9d
    5000529b:	45 89 e0             	mov    %r12d,%r8d
    5000529e:	b9 00 00 00 00       	mov    $0x0,%ecx
    500052a3:	ba 10 00 00 00       	mov    $0x10,%edx
    500052a8:	48 89 c7             	mov    %rax,%rdi
    500052ab:	e8 a5 fc ff ff       	callq  50004f55 <printi>
    500052b0:	48 83 c4 08          	add    $0x8,%rsp
    500052b4:	01 c3                	add    %eax,%ebx
    500052b6:	e9 82 01 00 00       	jmpq   5000543d <sgx_print+0x3bc>
    500052bb:	48 8b 45 c0          	mov    -0x40(%rbp),%rax
    500052bf:	0f b6 00             	movzbl (%rax),%eax
    500052c2:	3c 58                	cmp    $0x58,%al
    500052c4:	75 6e                	jne    50005334 <sgx_print+0x2b3>
    500052c6:	48 8b 45 b8          	mov    -0x48(%rbp),%rax
    500052ca:	8b 00                	mov    (%rax),%eax
    500052cc:	83 f8 30             	cmp    $0x30,%eax
    500052cf:	73 24                	jae    500052f5 <sgx_print+0x274>
    500052d1:	48 8b 45 b8          	mov    -0x48(%rbp),%rax
    500052d5:	48 8b 50 10          	mov    0x10(%rax),%rdx
    500052d9:	48 8b 45 b8          	mov    -0x48(%rbp),%rax
    500052dd:	8b 00                	mov    (%rax),%eax
    500052df:	89 c0                	mov    %eax,%eax
    500052e1:	48 01 d0             	add    %rdx,%rax
    500052e4:	48 8b 55 b8          	mov    -0x48(%rbp),%rdx
    500052e8:	8b 12                	mov    (%rdx),%edx
    500052ea:	8d 4a 08             	lea    0x8(%rdx),%ecx
    500052ed:	48 8b 55 b8          	mov    -0x48(%rbp),%rdx
    500052f1:	89 0a                	mov    %ecx,(%rdx)
    500052f3:	eb 14                	jmp    50005309 <sgx_print+0x288>
    500052f5:	48 8b 45 b8          	mov    -0x48(%rbp),%rax
    500052f9:	48 8b 40 08          	mov    0x8(%rax),%rax
    500052fd:	48 8d 48 08          	lea    0x8(%rax),%rcx
    50005301:	48 8b 55 b8          	mov    -0x48(%rbp),%rdx
    50005305:	48 89 4a 08          	mov    %rcx,0x8(%rdx)
    50005309:	8b 30                	mov    (%rax),%esi
    5000530b:	48 8b 45 c8          	mov    -0x38(%rbp),%rax
    5000530f:	6a 41                	pushq  $0x41
    50005311:	45 89 e9             	mov    %r13d,%r9d
    50005314:	45 89 e0             	mov    %r12d,%r8d
    50005317:	b9 00 00 00 00       	mov    $0x0,%ecx
    5000531c:	ba 10 00 00 00       	mov    $0x10,%edx
    50005321:	48 89 c7             	mov    %rax,%rdi
    50005324:	e8 2c fc ff ff       	callq  50004f55 <printi>
    50005329:	48 83 c4 08          	add    $0x8,%rsp
    5000532d:	01 c3                	add    %eax,%ebx
    5000532f:	e9 09 01 00 00       	jmpq   5000543d <sgx_print+0x3bc>
    50005334:	48 8b 45 c0          	mov    -0x40(%rbp),%rax
    50005338:	0f b6 00             	movzbl (%rax),%eax
    5000533b:	3c 75                	cmp    $0x75,%al
    5000533d:	75 6e                	jne    500053ad <sgx_print+0x32c>
    5000533f:	48 8b 45 b8          	mov    -0x48(%rbp),%rax
    50005343:	8b 00                	mov    (%rax),%eax
    50005345:	83 f8 30             	cmp    $0x30,%eax
    50005348:	73 24                	jae    5000536e <sgx_print+0x2ed>
    5000534a:	48 8b 45 b8          	mov    -0x48(%rbp),%rax
    5000534e:	48 8b 50 10          	mov    0x10(%rax),%rdx
    50005352:	48 8b 45 b8          	mov    -0x48(%rbp),%rax
    50005356:	8b 00                	mov    (%rax),%eax
    50005358:	89 c0                	mov    %eax,%eax
    5000535a:	48 01 d0             	add    %rdx,%rax
    5000535d:	48 8b 55 b8          	mov    -0x48(%rbp),%rdx
    50005361:	8b 12                	mov    (%rdx),%edx
    50005363:	8d 4a 08             	lea    0x8(%rdx),%ecx
    50005366:	48 8b 55 b8          	mov    -0x48(%rbp),%rdx
    5000536a:	89 0a                	mov    %ecx,(%rdx)
    5000536c:	eb 14                	jmp    50005382 <sgx_print+0x301>
    5000536e:	48 8b 45 b8          	mov    -0x48(%rbp),%rax
    50005372:	48 8b 40 08          	mov    0x8(%rax),%rax
    50005376:	48 8d 48 08          	lea    0x8(%rax),%rcx
    5000537a:	48 8b 55 b8          	mov    -0x48(%rbp),%rdx
    5000537e:	48 89 4a 08          	mov    %rcx,0x8(%rdx)
    50005382:	8b 30                	mov    (%rax),%esi
    50005384:	48 8b 45 c8          	mov    -0x38(%rbp),%rax
    50005388:	6a 61                	pushq  $0x61
    5000538a:	45 89 e9             	mov    %r13d,%r9d
    5000538d:	45 89 e0             	mov    %r12d,%r8d
    50005390:	b9 00 00 00 00       	mov    $0x0,%ecx
    50005395:	ba 0a 00 00 00       	mov    $0xa,%edx
    5000539a:	48 89 c7             	mov    %rax,%rdi
    5000539d:	e8 b3 fb ff ff       	callq  50004f55 <printi>
    500053a2:	48 83 c4 08          	add    $0x8,%rsp
    500053a6:	01 c3                	add    %eax,%ebx
    500053a8:	e9 90 00 00 00       	jmpq   5000543d <sgx_print+0x3bc>
    500053ad:	48 8b 45 c0          	mov    -0x40(%rbp),%rax
    500053b1:	0f b6 00             	movzbl (%rax),%eax
    500053b4:	3c 63                	cmp    $0x63,%al
    500053b6:	0f 85 81 00 00 00    	jne    5000543d <sgx_print+0x3bc>
    500053bc:	48 8b 45 b8          	mov    -0x48(%rbp),%rax
    500053c0:	8b 00                	mov    (%rax),%eax
    500053c2:	83 f8 30             	cmp    $0x30,%eax
    500053c5:	73 24                	jae    500053eb <sgx_print+0x36a>
    500053c7:	48 8b 45 b8          	mov    -0x48(%rbp),%rax
    500053cb:	48 8b 50 10          	mov    0x10(%rax),%rdx
    500053cf:	48 8b 45 b8          	mov    -0x48(%rbp),%rax
    500053d3:	8b 00                	mov    (%rax),%eax
    500053d5:	89 c0                	mov    %eax,%eax
    500053d7:	48 01 d0             	add    %rdx,%rax
    500053da:	48 8b 55 b8          	mov    -0x48(%rbp),%rdx
    500053de:	8b 12                	mov    (%rdx),%edx
    500053e0:	8d 4a 08             	lea    0x8(%rdx),%ecx
    500053e3:	48 8b 55 b8          	mov    -0x48(%rbp),%rdx
    500053e7:	89 0a                	mov    %ecx,(%rdx)
    500053e9:	eb 14                	jmp    500053ff <sgx_print+0x37e>
    500053eb:	48 8b 45 b8          	mov    -0x48(%rbp),%rax
    500053ef:	48 8b 40 08          	mov    0x8(%rax),%rax
    500053f3:	48 8d 48 08          	lea    0x8(%rax),%rcx
    500053f7:	48 8b 55 b8          	mov    -0x48(%rbp),%rdx
    500053fb:	48 89 4a 08          	mov    %rcx,0x8(%rdx)
    500053ff:	8b 00                	mov    (%rax),%eax
    50005401:	88 45 d0             	mov    %al,-0x30(%rbp)
    50005404:	c6 45 d1 00          	movb   $0x0,-0x2f(%rbp)
    50005408:	48 8d 75 d0          	lea    -0x30(%rbp),%rsi
    5000540c:	48 8b 45 c8          	mov    -0x38(%rbp),%rax
    50005410:	44 89 e9             	mov    %r13d,%ecx
    50005413:	44 89 e2             	mov    %r12d,%edx
    50005416:	48 89 c7             	mov    %rax,%rdi
    50005419:	e8 47 fa ff ff       	callq  50004e65 <prints>
    5000541e:	01 c3                	add    %eax,%ebx
    50005420:	eb 1b                	jmp    5000543d <sgx_print+0x3bc>
    50005422:	48 8b 45 c0          	mov    -0x40(%rbp),%rax
    50005426:	0f b6 00             	movzbl (%rax),%eax
    50005429:	0f be d0             	movsbl %al,%edx
    5000542c:	48 8b 45 c8          	mov    -0x38(%rbp),%rax
    50005430:	89 d6                	mov    %edx,%esi
    50005432:	48 89 c7             	mov    %rax,%rdi
    50005435:	e8 0d fa ff ff       	callq  50004e47 <printchar>
    5000543a:	83 c3 01             	add    $0x1,%ebx
    5000543d:	48 83 45 c0 01       	addq   $0x1,-0x40(%rbp)
    50005442:	48 8b 45 c0          	mov    -0x40(%rbp),%rax
    50005446:	0f b6 00             	movzbl (%rax),%eax
    50005449:	84 c0                	test   %al,%al
    5000544b:	0f 85 55 fc ff ff    	jne    500050a6 <sgx_print+0x25>
    50005451:	48 83 7d c8 00       	cmpq   $0x0,-0x38(%rbp)
    50005456:	74 0a                	je     50005462 <sgx_print+0x3e1>
    50005458:	48 8b 45 c8          	mov    -0x38(%rbp),%rax
    5000545c:	48 8b 00             	mov    (%rax),%rax
    5000545f:	c6 00 00             	movb   $0x0,(%rax)
    50005462:	89 d8                	mov    %ebx,%eax
    50005464:	48 8d 65 e0          	lea    -0x20(%rbp),%rsp
    50005468:	5b                   	pop    %rbx
    50005469:	41 5c                	pop    %r12
    5000546b:	41 5d                	pop    %r13
    5000546d:	41 5e                	pop    %r14
    5000546f:	5d                   	pop    %rbp
    50005470:	c3                   	retq   

0000000050005471 <printf>:
    50005471:	55                   	push   %rbp
    50005472:	48 89 e5             	mov    %rsp,%rbp
    50005475:	48 81 ec e0 00 00 00 	sub    $0xe0,%rsp
    5000547c:	48 89 b5 58 ff ff ff 	mov    %rsi,-0xa8(%rbp)
    50005483:	48 89 95 60 ff ff ff 	mov    %rdx,-0xa0(%rbp)
    5000548a:	48 89 8d 68 ff ff ff 	mov    %rcx,-0x98(%rbp)
    50005491:	4c 89 85 70 ff ff ff 	mov    %r8,-0x90(%rbp)
    50005498:	4c 89 8d 78 ff ff ff 	mov    %r9,-0x88(%rbp)
    5000549f:	84 c0                	test   %al,%al
    500054a1:	74 20                	je     500054c3 <printf+0x52>
    500054a3:	0f 29 45 80          	movaps %xmm0,-0x80(%rbp)
    500054a7:	0f 29 4d 90          	movaps %xmm1,-0x70(%rbp)
    500054ab:	0f 29 55 a0          	movaps %xmm2,-0x60(%rbp)
    500054af:	0f 29 5d b0          	movaps %xmm3,-0x50(%rbp)
    500054b3:	0f 29 65 c0          	movaps %xmm4,-0x40(%rbp)
    500054b7:	0f 29 6d d0          	movaps %xmm5,-0x30(%rbp)
    500054bb:	0f 29 75 e0          	movaps %xmm6,-0x20(%rbp)
    500054bf:	0f 29 7d f0          	movaps %xmm7,-0x10(%rbp)
    500054c3:	48 89 bd 28 ff ff ff 	mov    %rdi,-0xd8(%rbp)
    500054ca:	c7 85 38 ff ff ff 08 	movl   $0x8,-0xc8(%rbp)
    500054d1:	00 00 00 
    500054d4:	c7 85 3c ff ff ff 30 	movl   $0x30,-0xc4(%rbp)
    500054db:	00 00 00 
    500054de:	48 8d 45 10          	lea    0x10(%rbp),%rax
    500054e2:	48 89 85 40 ff ff ff 	mov    %rax,-0xc0(%rbp)
    500054e9:	48 8d 85 50 ff ff ff 	lea    -0xb0(%rbp),%rax
    500054f0:	48 89 85 48 ff ff ff 	mov    %rax,-0xb8(%rbp)
    500054f7:	48 8d 95 38 ff ff ff 	lea    -0xc8(%rbp),%rdx
    500054fe:	48 8b 85 28 ff ff ff 	mov    -0xd8(%rbp),%rax
    50005505:	48 89 c6             	mov    %rax,%rsi
    50005508:	bf 00 00 00 00       	mov    $0x0,%edi
    5000550d:	e8 6f fb ff ff       	callq  50005081 <sgx_print>
    50005512:	c9                   	leaveq 
    50005513:	c3                   	retq   

0000000050005514 <sgx_print_hex>:
    50005514:	55                   	push   %rbp
    50005515:	48 89 e5             	mov    %rsp,%rbp
    50005518:	48 83 ec 10          	sub    $0x10,%rsp
    5000551c:	48 89 7d f8          	mov    %rdi,-0x8(%rbp)
    50005520:	48 8b 45 f8          	mov    -0x8(%rbp),%rax
    50005524:	48 89 c6             	mov    %rax,%rsi
    50005527:	48 8d 3d f3 04 00 00 	lea    0x4f3(%rip),%rdi        # 50005a21 <strlen+0x1e6>
    5000552e:	b8 00 00 00 00       	mov    $0x0,%eax
    50005533:	e8 39 ff ff ff       	callq  50005471 <printf>
    50005538:	c9                   	leaveq 
    50005539:	c3                   	retq   

000000005000553a <tolower>:
    5000553a:	55                   	push   %rbp
    5000553b:	48 89 e5             	mov    %rsp,%rbp
    5000553e:	89 7d fc             	mov    %edi,-0x4(%rbp)
    50005541:	83 7d fc 40          	cmpl   $0x40,-0x4(%rbp)
    50005545:	7e 0e                	jle    50005555 <tolower+0x1b>
    50005547:	83 7d fc 5a          	cmpl   $0x5a,-0x4(%rbp)
    5000554b:	7f 08                	jg     50005555 <tolower+0x1b>
    5000554d:	8b 45 fc             	mov    -0x4(%rbp),%eax
    50005550:	83 c0 20             	add    $0x20,%eax
    50005553:	eb 03                	jmp    50005558 <tolower+0x1e>
    50005555:	8b 45 fc             	mov    -0x4(%rbp),%eax
    50005558:	5d                   	pop    %rbp
    50005559:	c3                   	retq   

000000005000555a <toupper>:
    5000555a:	55                   	push   %rbp
    5000555b:	48 89 e5             	mov    %rsp,%rbp
    5000555e:	89 7d fc             	mov    %edi,-0x4(%rbp)
    50005561:	83 7d fc 60          	cmpl   $0x60,-0x4(%rbp)
    50005565:	7e 0e                	jle    50005575 <toupper+0x1b>
    50005567:	83 7d fc 7a          	cmpl   $0x7a,-0x4(%rbp)
    5000556b:	7f 08                	jg     50005575 <toupper+0x1b>
    5000556d:	8b 45 fc             	mov    -0x4(%rbp),%eax
    50005570:	83 e8 20             	sub    $0x20,%eax
    50005573:	eb 03                	jmp    50005578 <toupper+0x1e>
    50005575:	8b 45 fc             	mov    -0x4(%rbp),%eax
    50005578:	5d                   	pop    %rbp
    50005579:	c3                   	retq   

000000005000557a <islower>:
    5000557a:	55                   	push   %rbp
    5000557b:	48 89 e5             	mov    %rsp,%rbp
    5000557e:	89 7d fc             	mov    %edi,-0x4(%rbp)
    50005581:	83 7d fc 60          	cmpl   $0x60,-0x4(%rbp)
    50005585:	7e 0d                	jle    50005594 <islower+0x1a>
    50005587:	83 7d fc 7a          	cmpl   $0x7a,-0x4(%rbp)
    5000558b:	7f 07                	jg     50005594 <islower+0x1a>
    5000558d:	b8 01 00 00 00       	mov    $0x1,%eax
    50005592:	eb 05                	jmp    50005599 <islower+0x1f>
    50005594:	b8 00 00 00 00       	mov    $0x0,%eax
    50005599:	5d                   	pop    %rbp
    5000559a:	c3                   	retq   

000000005000559b <isupper>:
    5000559b:	55                   	push   %rbp
    5000559c:	48 89 e5             	mov    %rsp,%rbp
    5000559f:	89 7d fc             	mov    %edi,-0x4(%rbp)
    500055a2:	83 7d fc 40          	cmpl   $0x40,-0x4(%rbp)
    500055a6:	7e 0d                	jle    500055b5 <isupper+0x1a>
    500055a8:	83 7d fc 5a          	cmpl   $0x5a,-0x4(%rbp)
    500055ac:	7f 07                	jg     500055b5 <isupper+0x1a>
    500055ae:	b8 01 00 00 00       	mov    $0x1,%eax
    500055b3:	eb 05                	jmp    500055ba <isupper+0x1f>
    500055b5:	b8 00 00 00 00       	mov    $0x0,%eax
    500055ba:	5d                   	pop    %rbp
    500055bb:	c3                   	retq   

00000000500055bc <isdigit>:
    500055bc:	55                   	push   %rbp
    500055bd:	48 89 e5             	mov    %rsp,%rbp
    500055c0:	89 7d fc             	mov    %edi,-0x4(%rbp)
    500055c3:	83 7d fc 2f          	cmpl   $0x2f,-0x4(%rbp)
    500055c7:	7e 0d                	jle    500055d6 <isdigit+0x1a>
    500055c9:	83 7d fc 39          	cmpl   $0x39,-0x4(%rbp)
    500055cd:	7f 07                	jg     500055d6 <isdigit+0x1a>
    500055cf:	b8 01 00 00 00       	mov    $0x1,%eax
    500055d4:	eb 05                	jmp    500055db <isdigit+0x1f>
    500055d6:	b8 00 00 00 00       	mov    $0x0,%eax
    500055db:	5d                   	pop    %rbp
    500055dc:	c3                   	retq   

00000000500055dd <isspace>:
    500055dd:	55                   	push   %rbp
    500055de:	48 89 e5             	mov    %rsp,%rbp
    500055e1:	89 7d fc             	mov    %edi,-0x4(%rbp)
    500055e4:	83 7d fc 20          	cmpl   $0x20,-0x4(%rbp)
    500055e8:	74 1e                	je     50005608 <isspace+0x2b>
    500055ea:	83 7d fc 09          	cmpl   $0x9,-0x4(%rbp)
    500055ee:	74 18                	je     50005608 <isspace+0x2b>
    500055f0:	83 7d fc 0a          	cmpl   $0xa,-0x4(%rbp)
    500055f4:	74 12                	je     50005608 <isspace+0x2b>
    500055f6:	83 7d fc 0b          	cmpl   $0xb,-0x4(%rbp)
    500055fa:	74 0c                	je     50005608 <isspace+0x2b>
    500055fc:	83 7d fc 0c          	cmpl   $0xc,-0x4(%rbp)
    50005600:	74 06                	je     50005608 <isspace+0x2b>
    50005602:	83 7d fc 0d          	cmpl   $0xd,-0x4(%rbp)
    50005606:	75 07                	jne    5000560f <isspace+0x32>
    50005608:	b8 01 00 00 00       	mov    $0x1,%eax
    5000560d:	eb 05                	jmp    50005614 <isspace+0x37>
    5000560f:	b8 00 00 00 00       	mov    $0x0,%eax
    50005614:	5d                   	pop    %rbp
    50005615:	c3                   	retq   

0000000050005616 <isalnum>:
    50005616:	55                   	push   %rbp
    50005617:	48 89 e5             	mov    %rsp,%rbp
    5000561a:	89 7d fc             	mov    %edi,-0x4(%rbp)
    5000561d:	83 7d fc 60          	cmpl   $0x60,-0x4(%rbp)
    50005621:	7e 06                	jle    50005629 <isalnum+0x13>
    50005623:	83 7d fc 7a          	cmpl   $0x7a,-0x4(%rbp)
    50005627:	7e 18                	jle    50005641 <isalnum+0x2b>
    50005629:	83 7d fc 40          	cmpl   $0x40,-0x4(%rbp)
    5000562d:	7e 06                	jle    50005635 <isalnum+0x1f>
    5000562f:	83 7d fc 5a          	cmpl   $0x5a,-0x4(%rbp)
    50005633:	7e 0c                	jle    50005641 <isalnum+0x2b>
    50005635:	83 7d fc 2f          	cmpl   $0x2f,-0x4(%rbp)
    50005639:	7e 0d                	jle    50005648 <isalnum+0x32>
    5000563b:	83 7d fc 39          	cmpl   $0x39,-0x4(%rbp)
    5000563f:	7f 07                	jg     50005648 <isalnum+0x32>
    50005641:	b8 01 00 00 00       	mov    $0x1,%eax
    50005646:	eb 05                	jmp    5000564d <isalnum+0x37>
    50005648:	b8 00 00 00 00       	mov    $0x0,%eax
    5000564d:	5d                   	pop    %rbp
    5000564e:	c3                   	retq   

000000005000564f <isxdigit>:
    5000564f:	55                   	push   %rbp
    50005650:	48 89 e5             	mov    %rsp,%rbp
    50005653:	89 7d fc             	mov    %edi,-0x4(%rbp)
    50005656:	83 7d fc 2f          	cmpl   $0x2f,-0x4(%rbp)
    5000565a:	7e 06                	jle    50005662 <isxdigit+0x13>
    5000565c:	83 7d fc 39          	cmpl   $0x39,-0x4(%rbp)
    50005660:	7e 18                	jle    5000567a <isxdigit+0x2b>
    50005662:	83 7d fc 60          	cmpl   $0x60,-0x4(%rbp)
    50005666:	7e 06                	jle    5000566e <isxdigit+0x1f>
    50005668:	83 7d fc 66          	cmpl   $0x66,-0x4(%rbp)
    5000566c:	7e 0c                	jle    5000567a <isxdigit+0x2b>
    5000566e:	83 7d fc 40          	cmpl   $0x40,-0x4(%rbp)
    50005672:	7e 0d                	jle    50005681 <isxdigit+0x32>
    50005674:	83 7d fc 46          	cmpl   $0x46,-0x4(%rbp)
    50005678:	7f 07                	jg     50005681 <isxdigit+0x32>
    5000567a:	b8 01 00 00 00       	mov    $0x1,%eax
    5000567f:	eb 05                	jmp    50005686 <isxdigit+0x37>
    50005681:	b8 00 00 00 00       	mov    $0x0,%eax
    50005686:	5d                   	pop    %rbp
    50005687:	c3                   	retq   

0000000050005688 <enclave_start>:
    50005688:	55                   	push   %rbp
    50005689:	48 89 e5             	mov    %rsp,%rbp
    5000568c:	53                   	push   %rbx
    5000568d:	48 83 ec 08          	sub    $0x8,%rsp
    50005691:	b8 00 00 00 00       	mov    $0x0,%eax
    50005696:	e8 71 ea ff ff       	callq  5000410c <enclave_main>
    5000569b:	b8 04 00 00 00       	mov    $0x4,%eax
    500056a0:	ba 00 00 00 00       	mov    $0x0,%edx
    500056a5:	48 89 d3             	mov    %rdx,%rbx
    500056a8:	89 c0                	mov    %eax,%eax
    500056aa:	48 89 db             	mov    %rbx,%rbx
    500056ad:	0f 01 d7             	enclu  
    500056b0:	48 83 c4 08          	add    $0x8,%rsp
    500056b4:	5b                   	pop    %rbx
    500056b5:	5d                   	pop    %rbp
    500056b6:	c3                   	retq   

00000000500056b7 <memset>:
    500056b7:	55                   	push   %rbp
    500056b8:	48 89 e5             	mov    %rsp,%rbp
    500056bb:	48 89 7d d8          	mov    %rdi,-0x28(%rbp)
    500056bf:	89 75 d4             	mov    %esi,-0x2c(%rbp)
    500056c2:	48 89 55 c8          	mov    %rdx,-0x38(%rbp)
    500056c6:	48 8b 45 d8          	mov    -0x28(%rbp),%rax
    500056ca:	48 89 45 f8          	mov    %rax,-0x8(%rbp)
    500056ce:	48 83 7d c8 07       	cmpq   $0x7,-0x38(%rbp)
    500056d3:	0f 86 40 01 00 00    	jbe    50005819 <memset+0x162>
    500056d9:	8b 45 d4             	mov    -0x2c(%rbp),%eax
    500056dc:	0f b6 c0             	movzbl %al,%eax
    500056df:	48 89 45 e8          	mov    %rax,-0x18(%rbp)
    500056e3:	48 8b 45 e8          	mov    -0x18(%rbp),%rax
    500056e7:	48 c1 e0 08          	shl    $0x8,%rax
    500056eb:	48 09 45 e8          	or     %rax,-0x18(%rbp)
    500056ef:	48 8b 45 e8          	mov    -0x18(%rbp),%rax
    500056f3:	48 c1 e0 10          	shl    $0x10,%rax
    500056f7:	48 09 45 e8          	or     %rax,-0x18(%rbp)
    500056fb:	48 8b 45 e8          	mov    -0x18(%rbp),%rax
    500056ff:	48 c1 e0 20          	shl    $0x20,%rax
    50005703:	48 09 45 e8          	or     %rax,-0x18(%rbp)
    50005707:	eb 13                	jmp    5000571c <memset+0x65>
    50005709:	48 8b 45 f8          	mov    -0x8(%rbp),%rax
    5000570d:	8b 55 d4             	mov    -0x2c(%rbp),%edx
    50005710:	88 10                	mov    %dl,(%rax)
    50005712:	48 83 45 f8 01       	addq   $0x1,-0x8(%rbp)
    50005717:	48 83 6d c8 01       	subq   $0x1,-0x38(%rbp)
    5000571c:	48 8b 45 f8          	mov    -0x8(%rbp),%rax
    50005720:	83 e0 07             	and    $0x7,%eax
    50005723:	48 85 c0             	test   %rax,%rax
    50005726:	75 e1                	jne    50005709 <memset+0x52>
    50005728:	48 8b 45 c8          	mov    -0x38(%rbp),%rax
    5000572c:	48 c1 e8 06          	shr    $0x6,%rax
    50005730:	48 89 45 f0          	mov    %rax,-0x10(%rbp)
    50005734:	e9 9a 00 00 00       	jmpq   500057d3 <memset+0x11c>
    50005739:	48 8b 45 f8          	mov    -0x8(%rbp),%rax
    5000573d:	48 8b 55 e8          	mov    -0x18(%rbp),%rdx
    50005741:	48 89 10             	mov    %rdx,(%rax)
    50005744:	48 8b 45 f8          	mov    -0x8(%rbp),%rax
    50005748:	48 83 c0 08          	add    $0x8,%rax
    5000574c:	48 89 c2             	mov    %rax,%rdx
    5000574f:	48 8b 45 e8          	mov    -0x18(%rbp),%rax
    50005753:	48 89 02             	mov    %rax,(%rdx)
    50005756:	48 8b 45 f8          	mov    -0x8(%rbp),%rax
    5000575a:	48 83 c0 10          	add    $0x10,%rax
    5000575e:	48 89 c2             	mov    %rax,%rdx
    50005761:	48 8b 45 e8          	mov    -0x18(%rbp),%rax
    50005765:	48 89 02             	mov    %rax,(%rdx)
    50005768:	48 8b 45 f8          	mov    -0x8(%rbp),%rax
    5000576c:	48 83 c0 18          	add    $0x18,%rax
    50005770:	48 89 c2             	mov    %rax,%rdx
    50005773:	48 8b 45 e8          	mov    -0x18(%rbp),%rax
    50005777:	48 89 02             	mov    %rax,(%rdx)
    5000577a:	48 8b 45 f8          	mov    -0x8(%rbp),%rax
    5000577e:	48 83 c0 20          	add    $0x20,%rax
    50005782:	48 89 c2             	mov    %rax,%rdx
    50005785:	48 8b 45 e8          	mov    -0x18(%rbp),%rax
    50005789:	48 89 02             	mov    %rax,(%rdx)
    5000578c:	48 8b 45 f8          	mov    -0x8(%rbp),%rax
    50005790:	48 83 c0 28          	add    $0x28,%rax
    50005794:	48 89 c2             	mov    %rax,%rdx
    50005797:	48 8b 45 e8          	mov    -0x18(%rbp),%rax
    5000579b:	48 89 02             	mov    %rax,(%rdx)
    5000579e:	48 8b 45 f8          	mov    -0x8(%rbp),%rax
    500057a2:	48 83 c0 30          	add    $0x30,%rax
    500057a6:	48 89 c2             	mov    %rax,%rdx
    500057a9:	48 8b 45 e8          	mov    -0x18(%rbp),%rax
    500057ad:	48 89 02             	mov    %rax,(%rdx)
    500057b0:	48 8b 45 f8          	mov    -0x8(%rbp),%rax
    500057b4:	48 83 c0 38          	add    $0x38,%rax
    500057b8:	48 89 c2             	mov    %rax,%rdx
    500057bb:	48 8b 45 e8          	mov    -0x18(%rbp),%rax
    500057bf:	48 89 02             	mov    %rax,(%rdx)
    500057c2:	48 8b 45 f8          	mov    -0x8(%rbp),%rax
    500057c6:	48 83 c0 40          	add    $0x40,%rax
    500057ca:	48 89 45 f8          	mov    %rax,-0x8(%rbp)
    500057ce:	48 83 6d f0 01       	subq   $0x1,-0x10(%rbp)
    500057d3:	48 83 7d f0 00       	cmpq   $0x0,-0x10(%rbp)
    500057d8:	0f 85 5b ff ff ff    	jne    50005739 <memset+0x82>
    500057de:	48 83 65 c8 3f       	andq   $0x3f,-0x38(%rbp)
    500057e3:	48 8b 45 c8          	mov    -0x38(%rbp),%rax
    500057e7:	48 c1 e8 03          	shr    $0x3,%rax
    500057eb:	48 89 45 f0          	mov    %rax,-0x10(%rbp)
    500057ef:	eb 1c                	jmp    5000580d <memset+0x156>
    500057f1:	48 8b 45 f8          	mov    -0x8(%rbp),%rax
    500057f5:	48 8b 55 e8          	mov    -0x18(%rbp),%rdx
    500057f9:	48 89 10             	mov    %rdx,(%rax)
    500057fc:	48 8b 45 f8          	mov    -0x8(%rbp),%rax
    50005800:	48 83 c0 08          	add    $0x8,%rax
    50005804:	48 89 45 f8          	mov    %rax,-0x8(%rbp)
    50005808:	48 83 6d f0 01       	subq   $0x1,-0x10(%rbp)
    5000580d:	48 83 7d f0 00       	cmpq   $0x0,-0x10(%rbp)
    50005812:	75 dd                	jne    500057f1 <memset+0x13a>
    50005814:	48 83 65 c8 07       	andq   $0x7,-0x38(%rbp)
    50005819:	eb 13                	jmp    5000582e <memset+0x177>
    5000581b:	48 8b 45 f8          	mov    -0x8(%rbp),%rax
    5000581f:	8b 55 d4             	mov    -0x2c(%rbp),%edx
    50005822:	88 10                	mov    %dl,(%rax)
    50005824:	48 83 45 f8 01       	addq   $0x1,-0x8(%rbp)
    50005829:	48 83 6d c8 01       	subq   $0x1,-0x38(%rbp)
    5000582e:	48 83 7d c8 00       	cmpq   $0x0,-0x38(%rbp)
    50005833:	75 e6                	jne    5000581b <memset+0x164>
    50005835:	48 8b 45 d8          	mov    -0x28(%rbp),%rax
    50005839:	5d                   	pop    %rbp
    5000583a:	c3                   	retq   

000000005000583b <strlen>:
    5000583b:	55                   	push   %rbp
    5000583c:	48 89 e5             	mov    %rsp,%rbp
    5000583f:	48 89 7d c8          	mov    %rdi,-0x38(%rbp)
    50005843:	48 8b 45 c8          	mov    -0x38(%rbp),%rax
    50005847:	48 89 45 f8          	mov    %rax,-0x8(%rbp)
    5000584b:	eb 23                	jmp    50005870 <strlen+0x35>
    5000584d:	48 8b 45 f8          	mov    -0x8(%rbp),%rax
    50005851:	0f b6 00             	movzbl (%rax),%eax
    50005854:	84 c0                	test   %al,%al
    50005856:	75 13                	jne    5000586b <strlen+0x30>
    50005858:	48 8b 55 f8          	mov    -0x8(%rbp),%rdx
    5000585c:	48 8b 45 c8          	mov    -0x38(%rbp),%rax
    50005860:	48 29 c2             	sub    %rax,%rdx
    50005863:	48 89 d0             	mov    %rdx,%rax
    50005866:	e9 a1 01 00 00       	jmpq   50005a0c <strlen+0x1d1>
    5000586b:	48 83 45 f8 01       	addq   $0x1,-0x8(%rbp)
    50005870:	48 8b 45 f8          	mov    -0x8(%rbp),%rax
    50005874:	83 e0 07             	and    $0x7,%eax
    50005877:	48 85 c0             	test   %rax,%rax
    5000587a:	75 d1                	jne    5000584d <strlen+0x12>
    5000587c:	48 8b 45 f8          	mov    -0x8(%rbp),%rax
    50005880:	48 89 45 f0          	mov    %rax,-0x10(%rbp)
    50005884:	b8 80 80 80 80       	mov    $0x80808080,%eax
    50005889:	48 89 45 e8          	mov    %rax,-0x18(%rbp)
    5000588d:	48 c7 45 e0 01 01 01 	movq   $0x1010101,-0x20(%rbp)
    50005894:	01 
    50005895:	48 8b 45 e8          	mov    -0x18(%rbp),%rax
    50005899:	48 c1 e0 20          	shl    $0x20,%rax
    5000589d:	48 09 45 e8          	or     %rax,-0x18(%rbp)
    500058a1:	48 8b 45 e0          	mov    -0x20(%rbp),%rax
    500058a5:	48 c1 e0 20          	shl    $0x20,%rax
    500058a9:	48 09 45 e0          	or     %rax,-0x20(%rbp)
    500058ad:	48 8b 45 f0          	mov    -0x10(%rbp),%rax
    500058b1:	48 8d 50 08          	lea    0x8(%rax),%rdx
    500058b5:	48 89 55 f0          	mov    %rdx,-0x10(%rbp)
    500058b9:	48 8b 00             	mov    (%rax),%rax
    500058bc:	48 89 45 d8          	mov    %rax,-0x28(%rbp)
    500058c0:	48 8b 45 d8          	mov    -0x28(%rbp),%rax
    500058c4:	48 2b 45 e0          	sub    -0x20(%rbp),%rax
    500058c8:	48 8b 55 d8          	mov    -0x28(%rbp),%rdx
    500058cc:	48 f7 d2             	not    %rdx
    500058cf:	48 21 d0             	and    %rdx,%rax
    500058d2:	48 23 45 e8          	and    -0x18(%rbp),%rax
    500058d6:	48 85 c0             	test   %rax,%rax
    500058d9:	0f 84 28 01 00 00    	je     50005a07 <strlen+0x1cc>
    500058df:	48 8b 45 f0          	mov    -0x10(%rbp),%rax
    500058e3:	48 83 e8 08          	sub    $0x8,%rax
    500058e7:	48 89 45 d0          	mov    %rax,-0x30(%rbp)
    500058eb:	48 8b 45 d0          	mov    -0x30(%rbp),%rax
    500058ef:	0f b6 00             	movzbl (%rax),%eax
    500058f2:	84 c0                	test   %al,%al
    500058f4:	75 13                	jne    50005909 <strlen+0xce>
    500058f6:	48 8b 55 d0          	mov    -0x30(%rbp),%rdx
    500058fa:	48 8b 45 c8          	mov    -0x38(%rbp),%rax
    500058fe:	48 29 c2             	sub    %rax,%rdx
    50005901:	48 89 d0             	mov    %rdx,%rax
    50005904:	e9 03 01 00 00       	jmpq   50005a0c <strlen+0x1d1>
    50005909:	48 8b 45 d0          	mov    -0x30(%rbp),%rax
    5000590d:	48 83 c0 01          	add    $0x1,%rax
    50005911:	0f b6 00             	movzbl (%rax),%eax
    50005914:	84 c0                	test   %al,%al
    50005916:	75 17                	jne    5000592f <strlen+0xf4>
    50005918:	48 8b 55 d0          	mov    -0x30(%rbp),%rdx
    5000591c:	48 8b 45 c8          	mov    -0x38(%rbp),%rax
    50005920:	48 29 c2             	sub    %rax,%rdx
    50005923:	48 89 d0             	mov    %rdx,%rax
    50005926:	48 83 c0 01          	add    $0x1,%rax
    5000592a:	e9 dd 00 00 00       	jmpq   50005a0c <strlen+0x1d1>
    5000592f:	48 8b 45 d0          	mov    -0x30(%rbp),%rax
    50005933:	48 83 c0 02          	add    $0x2,%rax
    50005937:	0f b6 00             	movzbl (%rax),%eax
    5000593a:	84 c0                	test   %al,%al
    5000593c:	75 17                	jne    50005955 <strlen+0x11a>
    5000593e:	48 8b 55 d0          	mov    -0x30(%rbp),%rdx
    50005942:	48 8b 45 c8          	mov    -0x38(%rbp),%rax
    50005946:	48 29 c2             	sub    %rax,%rdx
    50005949:	48 89 d0             	mov    %rdx,%rax
    5000594c:	48 83 c0 02          	add    $0x2,%rax
    50005950:	e9 b7 00 00 00       	jmpq   50005a0c <strlen+0x1d1>
    50005955:	48 8b 45 d0          	mov    -0x30(%rbp),%rax
    50005959:	48 83 c0 03          	add    $0x3,%rax
    5000595d:	0f b6 00             	movzbl (%rax),%eax
    50005960:	84 c0                	test   %al,%al
    50005962:	75 17                	jne    5000597b <strlen+0x140>
    50005964:	48 8b 55 d0          	mov    -0x30(%rbp),%rdx
    50005968:	48 8b 45 c8          	mov    -0x38(%rbp),%rax
    5000596c:	48 29 c2             	sub    %rax,%rdx
    5000596f:	48 89 d0             	mov    %rdx,%rax
    50005972:	48 83 c0 03          	add    $0x3,%rax
    50005976:	e9 91 00 00 00       	jmpq   50005a0c <strlen+0x1d1>
    5000597b:	48 8b 45 d0          	mov    -0x30(%rbp),%rax
    5000597f:	48 83 c0 04          	add    $0x4,%rax
    50005983:	0f b6 00             	movzbl (%rax),%eax
    50005986:	84 c0                	test   %al,%al
    50005988:	75 14                	jne    5000599e <strlen+0x163>
    5000598a:	48 8b 55 d0          	mov    -0x30(%rbp),%rdx
    5000598e:	48 8b 45 c8          	mov    -0x38(%rbp),%rax
    50005992:	48 29 c2             	sub    %rax,%rdx
    50005995:	48 89 d0             	mov    %rdx,%rax
    50005998:	48 83 c0 04          	add    $0x4,%rax
    5000599c:	eb 6e                	jmp    50005a0c <strlen+0x1d1>
    5000599e:	48 8b 45 d0          	mov    -0x30(%rbp),%rax
    500059a2:	48 83 c0 05          	add    $0x5,%rax
    500059a6:	0f b6 00             	movzbl (%rax),%eax
    500059a9:	84 c0                	test   %al,%al
    500059ab:	75 14                	jne    500059c1 <strlen+0x186>
    500059ad:	48 8b 55 d0          	mov    -0x30(%rbp),%rdx
    500059b1:	48 8b 45 c8          	mov    -0x38(%rbp),%rax
    500059b5:	48 29 c2             	sub    %rax,%rdx
    500059b8:	48 89 d0             	mov    %rdx,%rax
    500059bb:	48 83 c0 05          	add    $0x5,%rax
    500059bf:	eb 4b                	jmp    50005a0c <strlen+0x1d1>
    500059c1:	48 8b 45 d0          	mov    -0x30(%rbp),%rax
    500059c5:	48 83 c0 06          	add    $0x6,%rax
    500059c9:	0f b6 00             	movzbl (%rax),%eax
    500059cc:	84 c0                	test   %al,%al
    500059ce:	75 14                	jne    500059e4 <strlen+0x1a9>
    500059d0:	48 8b 55 d0          	mov    -0x30(%rbp),%rdx
    500059d4:	48 8b 45 c8          	mov    -0x38(%rbp),%rax
    500059d8:	48 29 c2             	sub    %rax,%rdx
    500059db:	48 89 d0             	mov    %rdx,%rax
    500059de:	48 83 c0 06          	add    $0x6,%rax
    500059e2:	eb 28                	jmp    50005a0c <strlen+0x1d1>
    500059e4:	48 8b 45 d0          	mov    -0x30(%rbp),%rax
    500059e8:	48 83 c0 07          	add    $0x7,%rax
    500059ec:	0f b6 00             	movzbl (%rax),%eax
    500059ef:	84 c0                	test   %al,%al
    500059f1:	75 14                	jne    50005a07 <strlen+0x1cc>
    500059f3:	48 8b 55 d0          	mov    -0x30(%rbp),%rdx
    500059f7:	48 8b 45 c8          	mov    -0x38(%rbp),%rax
    500059fb:	48 29 c2             	sub    %rax,%rdx
    500059fe:	48 89 d0             	mov    %rdx,%rax
    50005a01:	48 83 c0 07          	add    $0x7,%rax
    50005a05:	eb 05                	jmp    50005a0c <strlen+0x1d1>
    50005a07:	e9 a1 fe ff ff       	jmpq   500058ad <strlen+0x72>
    50005a0c:	5d                   	pop    %rbp
    50005a0d:	c3                   	retq   
