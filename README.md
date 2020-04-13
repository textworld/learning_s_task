# s_task 协程库的clion项目

该项目是为了学习s_task这个协程库而创建的，s_task库的地址为 https://github.com/xhawk18/s_task， 使用的版本是 https://github.com/xhawk18/s_task/commit/7b49dc599bb9bc62339ae9db7b46cc94c5bcc26f

## Table of content
- 协程环境初始化
- 协程创建
- 协程的切换

## 协程环境初始化

在main函数的一开始，调用了这么一个宏：
```
__init_async__
```

关于这个宏相关的有这么一些定义：
```c
#   define __await__      __awaiter_dummy__
#   define __async__      s_awaiter_t *__awaiter_dummy__
#   define __init_async__ s_awaiter_t *__awaiter_dummy__ = 0
```

`s_awaiter_t`是一个结构体，暂时撇下不管。`__awaiter_dummy__`这个实际就是一个变量名，和普通的变量`a`, `b`没有区别。调用`__init_async__`实际上就是声明了一个指向`s_awaiter_t`的指针变量，变量名叫做`__awaiter_dummy__`.`__await__`这个宏在后面使用得很多，但实际上只是指向了`__awaiter_dummy__`这个指针。  

接着再来看下函数`s_task_init_system`

TODO: 未完待续

## 协程创建
TODO

## 协程的切换
TODO