
#ifndef _FF_RPC_OPS_H_
#define _FF_RPC_OPS_H_

#include <assert.h>
#include <string>
using namespace std;

#include "base/ffslot.h"
#include "net/socket_i.h"
#include "base/fftype.h"
#include "net/codec.h"
#include "base/singleton.h"

namespace ff
{

#define BROKER_MASTER_NODE_ID   0
#define GEN_SERVICE_NAME(M, X, Y) snprintf(M, sizeof(M), "%s@%u", X, Y)
#define RECONNECT_TO_BROKER_TIMEOUT       1000//! ms

class ffslot_msg_arg: public ffslot_t::callback_arg_t
{
public:
    ffslot_msg_arg(const string& s_, socket_ptr_t sock_):
        body(s_),
        sock(sock_)
    {}
    virtual int type()
    {
        return TYPEID(ffslot_msg_arg);
    }
    string       body;
    socket_ptr_t sock;
};


class ffresponser_t
{
public:
    virtual ~ffresponser_t(){}
    virtual void response(uint32_t node_id_, uint32_t msg_id_, uint32_t callback_id_, const string& body_) = 0;
};

class ffslot_req_arg: public ffslot_t::callback_arg_t
{
public:
    ffslot_req_arg(const string& s_, uint32_t n_, uint32_t cb_id_, ffresponser_t* p):
        body(s_),
        node_id(n_),
        callback_id(cb_id_),
        responser(p)
    {}
    virtual int type()
    {
        return TYPEID(ffslot_req_arg);
    }
    string          body;
    uint32_t        node_id;//! 请求来自于那个node id
    uint32_t        callback_id;//! 回调函数标识id
    ffresponser_t*  responser;
};



class null_type_t: public ffmsg_t<null_type_t>
{
    void encode(){}
    void decode(){}
};

template<typename IN, typename OUT = null_type_t>
struct ffreq_t
{
    ffreq_t():
        node_id(0),
        callback_id(0),
        responser(NULL)
    {}
    IN              arg;
    uint32_t        node_id;
    uint32_t        callback_id;
    ffresponser_t*  responser;
    void response(OUT& out_)
    {
        responser->response(node_id, 0, callback_id, out_.encode_data());
    }
};

struct ffrpc_ops_t
{
    //! 接受broker 消息的回调函数
    template <typename R, typename T>
    static ffslot_t::callback_t* gen_callback(R (*)(T&, socket_ptr_t));
    template <typename R, typename CLASS_TYPE, typename T>
    static ffslot_t::callback_t* gen_callback(R (CLASS_TYPE::*)(T&, socket_ptr_t), CLASS_TYPE* obj_);
    
    //! broker client 注册接口
    template <typename R, typename IN, typename OUT>
    static ffslot_t::callback_t* gen_callback(R (*)(ffreq_t<IN, OUT>&));
    template <typename R, typename CLASS_TYPE, typename IN, typename OUT>
    static ffslot_t::callback_t* gen_callback(R (CLASS_TYPE::*)(ffreq_t<IN, OUT>&), CLASS_TYPE* obj);

    //! ffrpc call接口的回调函数参数
    //! 如果绑定回调函数的时候，有时需要一些临时参数被保存直到回调函数被调用
    template <typename R, typename CLASS_TYPE, typename IN, typename OUT, typename FUNC_ARG1, typename ARG1>
    static ffslot_t::callback_t* gen_callback(R (CLASS_TYPE::*func_)(ffreq_t<IN, OUT>&, FUNC_ARG1), CLASS_TYPE* obj_, ARG1 arg1_);
    template <typename R, typename CLASS_TYPE, typename IN, typename OUT, typename FUNC_ARG1, typename ARG1,
              typename FUNC_ARG2, typename ARG2>
    static ffslot_t::callback_t* gen_callback(R (CLASS_TYPE::*func_)(ffreq_t<IN, OUT>&, FUNC_ARG1, FUNC_ARG2),
                                                CLASS_TYPE* obj_, ARG1 arg1_, ARG2 arg2_);
    template <typename R, typename CLASS_TYPE, typename IN, typename OUT, typename FUNC_ARG1, typename ARG1,
              typename FUNC_ARG2, typename ARG2, typename FUNC_ARG3, typename ARG3>
    static ffslot_t::callback_t* gen_callback(R (CLASS_TYPE::*func_)(ffreq_t<IN, OUT>&, FUNC_ARG1, FUNC_ARG2, FUNC_ARG3),
                                                CLASS_TYPE* obj_, ARG1 arg1_, ARG2 arg2_, ARG3 arg3_);
    template <typename R, typename CLASS_TYPE, typename IN, typename OUT, typename FUNC_ARG1, typename ARG1,
              typename FUNC_ARG2, typename ARG2, typename FUNC_ARG3, typename ARG3, typename FUNC_ARG4, typename ARG4>
    static ffslot_t::callback_t* gen_callback(R (CLASS_TYPE::*func_)(ffreq_t<IN, OUT>&, FUNC_ARG1, FUNC_ARG2, FUNC_ARG3, FUNC_ARG4),
                                                CLASS_TYPE* obj_, ARG1 arg1_, ARG2 arg2_, ARG3 arg3_, ARG4 arg4_);
    
};

template <typename R, typename T>
ffslot_t::callback_t* ffrpc_ops_t::gen_callback(R (*func_)(T&, socket_ptr_t))
{
    struct lambda_cb: public ffslot_t::callback_t
    {
        typedef R (*func_t)(T&, socket_ptr_t);
        lambda_cb(func_t func_):m_func(func_){}
        virtual void exe(ffslot_t::callback_arg_t* args_)
        {
            if (args_->type() != TYPEID(ffslot_msg_arg))
            {
                return;
            }
            ffslot_msg_arg* msg_data = (ffslot_msg_arg*)args_;
            T msg;
            msg.decode(msg_data->body);
            m_func(msg, msg_data->sock);
        }
        virtual ffslot_t::callback_t* fork() { return new lambda_cb(m_func); }
        func_t m_func;
    };
    return new lambda_cb(func_);
}
template <typename R, typename CLASS_TYPE, typename T>
ffslot_t::callback_t* ffrpc_ops_t::gen_callback(R (CLASS_TYPE::*func_)(T&, socket_ptr_t), CLASS_TYPE* obj_)
{
    struct lambda_cb: public ffslot_t::callback_t
    {
        typedef R (CLASS_TYPE::*func_t)(T&, socket_ptr_t);
        lambda_cb(func_t func_, CLASS_TYPE* obj_):m_func(func_), m_obj(obj_){}
        virtual void exe(ffslot_t::callback_arg_t* args_)
        {
            if (args_->type() != TYPEID(ffslot_msg_arg))
            {
                return;
            }
            ffslot_msg_arg* msg_data = (ffslot_msg_arg*)args_;
            T msg;
            msg.decode_data(msg_data->body);
            (m_obj->*(m_func))(msg, msg_data->sock);
        }
        virtual ffslot_t::callback_t* fork() { return new lambda_cb(m_func, m_obj); }
        func_t      m_func;
        CLASS_TYPE* m_obj;
    };
    return new lambda_cb(func_, obj_);
}

//! 注册接口
template <typename R, typename IN, typename OUT>
ffslot_t::callback_t* ffrpc_ops_t::gen_callback(R (*func_)(ffreq_t<IN, OUT>&))
{
    struct lambda_cb: public ffslot_t::callback_t
    {
        typedef R (*func_t)(ffreq_t<IN, OUT>&);
        lambda_cb(func_t func_):m_func(func_){}
        virtual void exe(ffslot_t::callback_arg_t* args_)
        {
            if (args_->type() != TYPEID(ffslot_req_arg))
            {
                return;
            }
            ffslot_req_arg* msg_data = (ffslot_req_arg*)args_;
            ffreq_t<IN, OUT> req;
            req.arg.decode_data(msg_data->body);
            req.node_id = msg_data->node_id;
            req.callback_id = msg_data->callback_id;
            req.responser = msg_data->responser;
            m_func(req);
        }
        virtual ffslot_t::callback_t* fork() { return new lambda_cb(m_func); }
        func_t m_func;
    };
    return new lambda_cb(func_);
}
template <typename R, typename CLASS_TYPE, typename IN, typename OUT>
ffslot_t::callback_t* ffrpc_ops_t::gen_callback(R (CLASS_TYPE::*func_)(ffreq_t<IN, OUT>&), CLASS_TYPE* obj_)
{
    struct lambda_cb: public ffslot_t::callback_t
    {
        typedef R (CLASS_TYPE::*func_t)(ffreq_t<IN, OUT>&);
        lambda_cb(func_t func_, CLASS_TYPE* obj_):m_func(func_), m_obj(obj_){}
        virtual void exe(ffslot_t::callback_arg_t* args_)
        {
            if (args_->type() != TYPEID(ffslot_req_arg))
            {
                return;
            }
            ffslot_req_arg* msg_data = (ffslot_req_arg*)args_;
            ffreq_t<IN, OUT> req;
            req.arg.decode_data(msg_data->body);
            req.node_id = msg_data->node_id;
            req.callback_id = msg_data->callback_id;
            req.responser = msg_data->responser;
            (m_obj->*(m_func))(req);
        }
        virtual ffslot_t::callback_t* fork() { return new lambda_cb(m_func, m_obj); }
        func_t      m_func;
        CLASS_TYPE* m_obj;
    };
    return new lambda_cb(func_, obj_);
}

//! 如果绑定回调函数的时候，有时需要一些临时参数被保存直到回调函数被调用
template <typename R, typename CLASS_TYPE, typename IN, typename OUT, typename FUNC_ARG1, typename ARG1>
ffslot_t::callback_t* ffrpc_ops_t::gen_callback(R (CLASS_TYPE::*func_)(ffreq_t<IN, OUT>&, FUNC_ARG1), CLASS_TYPE* obj_, ARG1 arg1_)
{
    struct lambda_cb: public ffslot_t::callback_t
    {
        typedef R (CLASS_TYPE::*func_t)(ffreq_t<IN, OUT>&, FUNC_ARG1);
        lambda_cb(func_t func_, CLASS_TYPE* obj_, const ARG1& arg1_):
            m_func(func_), m_obj(obj_), m_arg1(arg1_){}
        virtual void exe(ffslot_t::callback_arg_t* args_)
        {
            if (args_->type() != TYPEID(ffslot_req_arg))
            {
                return;
            }
            ffslot_req_arg* msg_data = (ffslot_req_arg*)args_;
            ffreq_t<IN, OUT> req;
            req.arg.decode_data(msg_data->body);
            req.node_id = msg_data->node_id;
            req.callback_id = msg_data->callback_id;
            req.responser = msg_data->responser;
            (m_obj->*(m_func))(req, m_arg1);
        }
        virtual ffslot_t::callback_t* fork() { return new lambda_cb(m_func, m_obj, m_arg1); }
        func_t      m_func;
        CLASS_TYPE* m_obj;
        typename fftraits_t<ARG1>::value_t m_arg1;
    };
    return new lambda_cb(func_, obj_, arg1_);
}

//! 如果绑定回调函数的时候，有时需要一些临时参数被保存直到回调函数被调用
template <typename R, typename CLASS_TYPE, typename IN, typename OUT, typename FUNC_ARG1, typename ARG1, typename FUNC_ARG2, typename ARG2>
ffslot_t::callback_t* ffrpc_ops_t::gen_callback(R (CLASS_TYPE::*func_)(ffreq_t<IN, OUT>&, FUNC_ARG1, FUNC_ARG2),
                                                CLASS_TYPE* obj_, ARG1 arg1_, ARG2 arg2_)
{
    struct lambda_cb: public ffslot_t::callback_t
    {
        typedef R (CLASS_TYPE::*func_t)(ffreq_t<IN, OUT>&, FUNC_ARG1, FUNC_ARG2);
        lambda_cb(func_t func_, CLASS_TYPE* obj_, const ARG1& arg1_, const ARG2& arg2_):
            m_func(func_), m_obj(obj_), m_arg1(arg1_), m_arg2(arg2_)
        {}
        virtual void exe(ffslot_t::callback_arg_t* args_)
        {
            if (args_->type() != TYPEID(ffslot_req_arg))
            {
                return;
            }
            ffslot_req_arg* msg_data = (ffslot_req_arg*)args_;
            ffreq_t<IN, OUT> req;
            req.arg.decode_data(msg_data->body);
            req.node_id = msg_data->node_id;
            req.callback_id = msg_data->callback_id;
            req.responser = msg_data->responser;
            (m_obj->*(m_func))(req, m_arg1, m_arg2);
        }
        virtual ffslot_t::callback_t* fork() { return new lambda_cb(m_func, m_obj, m_arg1, m_arg2); }
        func_t      m_func;
        CLASS_TYPE* m_obj;
        typename fftraits_t<ARG1>::value_t m_arg1;
        typename fftraits_t<ARG2>::value_t m_arg2;
    };
    return new lambda_cb(func_, obj_, arg1_, arg2_);
}

//! 如果绑定回调函数的时候，有时需要一些临时参数被保存直到回调函数被调用
template <typename R, typename CLASS_TYPE, typename IN, typename OUT, typename FUNC_ARG1, typename ARG1, typename FUNC_ARG2, typename ARG2,
          typename FUNC_ARG3, typename ARG3>
ffslot_t::callback_t* ffrpc_ops_t::gen_callback(R (CLASS_TYPE::*func_)(ffreq_t<IN, OUT>&, FUNC_ARG1, FUNC_ARG2, FUNC_ARG3),
                                                CLASS_TYPE* obj_, ARG1 arg1_, ARG2 arg2_, ARG3 arg3_)
{
    struct lambda_cb: public ffslot_t::callback_t
    {
        typedef R (CLASS_TYPE::*func_t)(ffreq_t<IN, OUT>&, FUNC_ARG1, FUNC_ARG2, FUNC_ARG3);
        lambda_cb(func_t func_, CLASS_TYPE* obj_, const ARG1& arg1_, const ARG2& arg2_, const ARG3& arg3_):
            m_func(func_), m_obj(obj_), m_arg1(arg1_), m_arg2(arg2_), m_arg3(arg3_)
        {}
        virtual void exe(ffslot_t::callback_arg_t* args_)
        {
            if (args_->type() != TYPEID(ffslot_req_arg))
            {
                return;
            }
            ffslot_req_arg* msg_data = (ffslot_req_arg*)args_;
            ffreq_t<IN, OUT> req;
            req.arg.decode_data(msg_data->body);
            req.node_id = msg_data->node_id;
            req.callback_id = msg_data->callback_id;
            req.responser = msg_data->responser;
            (m_obj->*(m_func))(req, m_arg1, m_arg2, m_arg3);
        }
        virtual ffslot_t::callback_t* fork() { return new lambda_cb(m_func, m_obj, m_arg1, m_arg2, m_arg3); }
        func_t      m_func;
        CLASS_TYPE* m_obj;
        typename fftraits_t<ARG1>::value_t m_arg1;
        typename fftraits_t<ARG2>::value_t m_arg2;
        typename fftraits_t<ARG3>::value_t m_arg3;
    };
    return new lambda_cb(func_, obj_, arg1_, arg2_, arg3_);
}

//! 如果绑定回调函数的时候，有时需要一些临时参数被保存直到回调函数被调用
template <typename R, typename CLASS_TYPE, typename IN, typename OUT, typename FUNC_ARG1, typename ARG1, typename FUNC_ARG2, typename ARG2,
          typename FUNC_ARG3, typename ARG3, typename FUNC_ARG4, typename ARG4>
ffslot_t::callback_t* ffrpc_ops_t::gen_callback(R (CLASS_TYPE::*func_)(ffreq_t<IN, OUT>&, FUNC_ARG1, FUNC_ARG2, FUNC_ARG3, FUNC_ARG4),
                                                CLASS_TYPE* obj_, ARG1 arg1_, ARG2 arg2_, ARG3 arg3_, ARG4 arg4_)
{
    struct lambda_cb: public ffslot_t::callback_t
    {
        typedef R (CLASS_TYPE::*func_t)(ffreq_t<IN, OUT>&, FUNC_ARG1, FUNC_ARG2, FUNC_ARG3, FUNC_ARG4);
        lambda_cb(func_t func_, CLASS_TYPE* obj_, const ARG1& arg1_, const ARG2& arg2_, const ARG3& arg3_, const ARG4& arg4_):
            m_func(func_), m_obj(obj_), m_arg1(arg1_), m_arg2(arg2_), m_arg3(arg3_), m_arg4(arg4_)
        {}
        virtual void exe(ffslot_t::callback_arg_t* args_)
        {
            if (args_->type() != TYPEID(ffslot_req_arg))
            {
                return;
            }
            ffslot_req_arg* msg_data = (ffslot_req_arg*)args_;
            ffreq_t<IN, OUT> req;
            req.arg.decode_data(msg_data->body);
            req.node_id = msg_data->node_id;
            req.callback_id = msg_data->callback_id;
            req.responser = msg_data->responser;
            (m_obj->*(m_func))(req, m_arg1, m_arg2, m_arg3, m_arg4);
        }
        virtual ffslot_t::callback_t* fork() { return new lambda_cb(m_func, m_obj, m_arg1, m_arg2, m_arg3, m_arg4); }
        func_t      m_func;
        CLASS_TYPE* m_obj;
        typename fftraits_t<ARG1>::value_t m_arg1;
        typename fftraits_t<ARG2>::value_t m_arg2;
        typename fftraits_t<ARG3>::value_t m_arg3;
        typename fftraits_t<ARG4>::value_t m_arg4;
    };
    return new lambda_cb(func_, obj_, arg1_, arg2_, arg3_, arg4_);
}


enum ffrpc_cmd_def_e
{
    BROKER_SLAVE_REGISTER  = 1,
    BROKER_CLIENT_REGISTER,
    BROKER_ROUTE_MSG,
    BROKER_SYNC_DATA_MSG,
    BROKER_TO_CLIENT_MSG,
    CLIENT_REGISTER_TO_SLAVE_BROKER,
};


//! 向broker master 注册slave
struct register_slave_broker_t
{
    struct in_t: public ffmsg_t<in_t>
    {
        void encode()
        {
            encoder() << host;
        }
        void decode()
        {
            decoder()>> host;
        }
        string          host;
    };
};

//! 向broker master 注册client
struct register_broker_client_t
{
    struct in_t: public ffmsg_t<in_t>
    {
        void encode()
        {
            encoder() << service_name << msg_names;
        }
        void decode()
        {
            decoder() >> service_name >> msg_names;
        }
        string                      service_name;
        std::set<string>            msg_names;
    };
};
//! 向broker slave 注册client
struct register_client_to_slave_broker_t
{
    struct in_t: public ffmsg_t<in_t>
    {
        void encode()
        {
            encoder() << node_id;
        }
        void decode()
        {
            decoder() >> node_id;
        }
        uint32_t                    node_id;//! master分配client的node id
    };
};

struct broker_sync_all_registered_data_t
{
    //! 记录每个broker slave 的接口信息
    struct slave_broker_info_t: public ffmsg_t<slave_broker_info_t>
    {
        void encode()
        {
            encoder() << host;
        }
        void decode()
        {
            decoder() >> host;
        }
        string          host;
    };

    struct broker_client_info_t: public ffmsg_t<broker_client_info_t>
    {
        void encode()
        {
            encoder() << bind_broker_id << service_name;
        }
        void decode()
        {
            decoder() >> bind_broker_id >> service_name;
        }
        //! 被绑定的节点broker node id
        uint32_t bind_broker_id;
        string   service_name;
    };
    struct out_t: public ffmsg_t<out_t>
    {
        out_t():
            node_id(0)
        {}
        void encode()
        {
            encoder() << node_id << msg2id << slave_broker_info << broker_client_info;
        }
        void decode()
        {
            decoder() >> node_id >> msg2id >> slave_broker_info >> broker_client_info;
        }
        uint32_t                                node_id;//! 被分配的node id
        map<string, uint32_t>                   msg2id; //! 消息名称对应的消息id 值
        //!记录所有的broker slave 信息
        map<uint32_t, slave_broker_info_t>      slave_broker_info;//! node id -> broker slave
        //! 记录所有服务/接口信息
        map<uint32_t, broker_client_info_t>     broker_client_info;//! node id -> service
    };
};

struct broker_route_t//!broker 转发消息
{
    struct in_t: public ffmsg_t<in_t>
    {
        void encode()
        {
            encoder() << from_node_id << dest_node_id << msg_id << callback_id << body;
        }
        void decode()
        {
            decoder() >> from_node_id >> dest_node_id >> msg_id >> callback_id >> body;
        }
        uint32_t                    from_node_id;//! 来自哪个节点
        uint32_t                    dest_node_id;//! 需要转发到哪个节点上
        uint32_t                    msg_id;//! 调用的是哪个接口
        uint32_t                    callback_id;
        string                      body;
    };
};

//! 用于broker 和 rpc 在内存间投递消息
class ffrpc_t;
class ffbroker_t;
struct ffrpc_memory_route_t
{
    struct dest_node_info_t
    {
        dest_node_info_t():
            ffrpc(NULL),
            ffbroker(NULL)
        {}
        ffrpc_t*        ffrpc;
        ffbroker_t*     ffbroker;
    };
    int add_node(uint32_t node_id_, ffrpc_t* ffrpc_);
    int add_node(uint32_t node_id_, ffbroker_t* ffbroker_);
    int del_node(uint32_t node_id_);

    //! 判断目标节点是否在同一进程中
    bool is_same_process(uint32_t node_id_);
    //! broker 转发消息到rpc client
    int broker_route_to_client(broker_route_t::in_t& msg_);
    //! client 转发消息到 broker, 再由broker转发到client
    int client_route_to_broker(broker_route_t::in_t& msg_);

    //! 所有已经注册的本进程的节点
    vector<uint32_t> get_node_same_process();
    map<uint32_t/*node id*/, dest_node_info_t>      m_node_info;
};

}

#endif
