# socket内核对象

## 用户创建socket

```c
#include<sys/socket.h>

int main()
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    // ...
    return 0;
}
```

## Linux内核做了什么
### 通过pf->create创建
```c
// linux-hwe-6.8-6.8.0/net/socket.c

SYSCALL_DEFINE3(socket, int, family, int, type, int, protocol)
{
    return __sys_socket(family, type, protocol);
}

//  __sys_socket
//      __sys_socket_create
//          sock_create
//              __sock_create

int __sock_create(struct net *net, int family, int type, int protocol,
			 struct socket **res, int kern)
{
    struct socket *sock;
	sock = sock_alloc();
	pf = rcu_dereference(net_families[family]);
	err = pf->create(net, sock, protocol, kern);
}
```

### pf->create做了什么
```c
// linux-hwe-6.8-6.8.0/net/ipv4/af_inet.c

static const struct net_proto_family inet_family_ops = {
	.family = PF_INET,
	.create = inet_create,
	.owner	= THIS_MODULE,
};


static int inet_create(struct net *net, struct socket *sock, int protocol, int kern)
{
	struct sock *sk;
    list_for_each_entry_rcu(answer, &inetsw[sock->type], list) {
        // ...
    }
    sock->ops = answer->ops;
	answer_prot = answer->prot;
	sk = sk_alloc(net, PF_INET, GFP_KERNEL, answer_prot, kern);
	inet = inet_sk(sk);
	sock_init_data(sock, sk);
}
```

### answer是什么
```c
// linux-hwe-6.8-6.8.0/net/ipv4/af_inet.c

static struct inet_protosw inetsw_array[] =
{
	{
		.type =       SOCK_STREAM,
		.protocol =   IPPROTO_TCP,
		.prot =       &tcp_prot,
		.ops =        &inet_stream_ops,
		.flags =      INET_PROTOSW_PERMANENT |
			      INET_PROTOSW_ICSK,
	},
}
```

## 最后看看socket内核数据结构
```c
// linux-hwe-6.8-6.8.0/include/linux/net.h

struct socket {
	socket_state		state;
	short			type;
	unsigned long		flags;
	struct file		*file;
	struct sock		*sk;
	const struct proto_ops	*ops; /* Might change with IPV6_ADDRFORM or MPTCP. */
	struct socket_wq	wq;
};

```

```c
// linux-hwe-6.8-6.8.0/include/linux/sock.h
struct sock {
	struct sock_common	__sk_common;
#define sk_prot			__sk_common.skc_prot
    struct sk_buff_head	sk_receive_queue;
    union {
		struct socket_wq __rcu	*sk_wq;
		/* private: */
		struct socket_wq	*sk_wq_raw;
		/* public: */
	};
	void		(*sk_state_change)(struct sock *sk);
	void		(*sk_data_ready)(struct sock *sk);
	void		(*sk_write_space)(struct sock *sk);
	void		(*sk_error_report)(struct sock *sk);
	int			(*sk_backlog_rcv)(struct sock *sk, struct sk_buff *skb);
};

struct sock_common {
	struct proto		*skc_prot;
};

```
### sockct->ops = inet_stream_ops
```c
// linux-hwe-6.8-6.8.0/net/ipv4_af_inet.c
const struct proto_ops inet_stream_ops = {
	.family		   = PF_INET,
	.owner		   = THIS_MODULE,
	.release	   = inet_release,
	.bind		   = inet_bind,
	.connect	   = inet_stream_connect,
	.socketpair	   = sock_no_socketpair,
	.accept		   = inet_accept,
	.getname	   = inet_getname,
	.poll		   = tcp_poll,
	.ioctl		   = inet_ioctl,
	.gettstamp	   = sock_gettstamp,
	.listen		   = inet_listen,
	.shutdown	   = inet_shutdown,
	.setsockopt	   = sock_common_setsockopt,
	.getsockopt	   = sock_common_getsockopt,
	.sendmsg	   = inet_sendmsg,
	.recvmsg	   = inet_recvmsg,
};
```

### socket->sk->sk_prot = tcp_prot
```c
// linux-hwe-6.8-6.8.0/net/ipv4/tcp_ipv4.c
struct proto tcp_prot = {
	.name			= "TCP",
	.owner			= THIS_MODULE,
	.close			= tcp_close,
	.pre_connect		= tcp_v4_pre_connect,
	.connect		= tcp_v4_connect,
	.disconnect		= tcp_disconnect,
	.accept			= inet_csk_accept,
	.ioctl			= tcp_ioctl,
	.init			= tcp_v4_init_sock,
	.destroy		= tcp_v4_destroy_sock,
	.shutdown		= tcp_shutdown,
	.setsockopt		= tcp_setsockopt,
	.getsockopt		= tcp_getsockopt,
	.bpf_bypass_getsockopt	= tcp_bpf_bypass_getsockopt,
	.keepalive		= tcp_set_keepalive,
	.recvmsg		= tcp_recvmsg,
	.sendmsg		= tcp_sendmsg,
	.splice_eof		= tcp_splice_eof,
	.backlog_rcv		= tcp_v4_do_rcv,
	.release_cb		= tcp_release_cb,
	.hash			= inet_hash,
	.unhash			= inet_unhash,
	.get_port		= inet_csk_get_port,
	.put_port		= inet_put_port,
#ifdef CONFIG_BPF_SYSCALL
	.psock_update_sk_prot	= tcp_bpf_update_proto,
#endif
	.enter_memory_pressure	= tcp_enter_memory_pressure,
	.leave_memory_pressure	= tcp_leave_memory_pressure,
	.stream_memory_free	= tcp_stream_memory_free,
	.sockets_allocated	= &tcp_sockets_allocated,
	.orphan_count		= &tcp_orphan_count,

	.memory_allocated	= &tcp_memory_allocated,
	.per_cpu_fw_alloc	= &tcp_memory_per_cpu_fw_alloc,

	.memory_pressure	= &tcp_memory_pressure,
	.sysctl_mem		= sysctl_tcp_mem,
	.sysctl_wmem_offset	= offsetof(struct net, ipv4.sysctl_tcp_wmem),
	.sysctl_rmem_offset	= offsetof(struct net, ipv4.sysctl_tcp_rmem),
	.max_header		= MAX_TCP_HEADER,
	.obj_size		= sizeof(struct tcp_sock),
	.slab_flags		= SLAB_TYPESAFE_BY_RCU,
	.twsk_prot		= &tcp_timewait_sock_ops,
	.rsk_prot		= &tcp_request_sock_ops,
	.h.hashinfo		= NULL,
	.no_autobind		= true,
	.diag_destroy		= tcp_abort,
};
```

### socket->sk->sk_data_ready = sock_def_readable
```c
// linux-hwe-6.8-6.8.0/net/core/sock.c
void sock_init_data_uid(struct socket *sock, struct sock *sk, kuid_t uid)
{

	sk->sk_state_change	=	sock_def_wakeup;
	sk->sk_data_ready	=	sock_def_readable;
	sk->sk_write_space	=	sock_def_write_space;
	sk->sk_error_report	=	sock_def_error_report;
	sk->sk_destruct		=	sock_def_destruct;
}
```

## 小结
```c
struct socket tcp_socket = {
    .sk = &sk,
    // .sk.sk_prot = &tcp_prot,
    // .sk.sk_data_ready = &sock_def_readable,
    // .sk.sk_receive_queue
    // .sk.sk_wq
    .ops = &inet_stream_ops,
}

```