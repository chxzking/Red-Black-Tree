#ifndef __RB_TREE_H__
#define __RB_TREE_H__
/************************************************************************************************
*	红黑树前置定义
************************************************************************************************/

#define ENABLE_RBTREE_ERROR_CODE_PRINT									//红黑树错误码打印使能。开启后可以使用红黑树的 rbTree_ErrorCodePrint 函数通过错误码打印错误内容

/*
*	@brief 配置红黑树的删除策略
*/
#define TIME_PRIORITY_DELETE_TACTICS		0							//时间优先，将使用递归方案，过深的树可能导致栈溢出
#define	BALANCE_DELETE_TACTICS				1							//均衡策略，使用堆区模拟栈方案，速度较块，内存溢出的可能性较小。
#define MEM_PRIORITY_DELETE_TACTICS			2							//内存优先，删除过程中将不会使用几乎任何额外内存开销，但是过深的树可能导致耗时较长
//配置接口
#define DELETE_TACTICS_CONFIG			BALANCE_DELETE_TACTICS			//红黑树删除策略配置接口，默认使用时间优先	

/**
*	@brief		红黑树管理器
*/
typedef struct rbTreeManager_t	rbTreeManager_t;
//查找规则
/**
*	@brief	红黑树节点匹配规则句柄。您应当传入该函数指针,红黑树将使用此规则对节点进行匹配，该规则将被使用在查找匹配、节点插入方向确定等操作中。
*	
*	@param
*		@param	const void* reference_node		//参考对象
*		@param  const void* rbTree_node			//红黑树节点对象，此对象将会从节点中提取，用于和 reference_node 进行比较
*	
*	@retval
*		@retval		如果匹配成功，则返回 [0]
*		@retval		如果匹配失败，需要向红黑树的[左]子树继续匹配则返回负数，如[-1]
* 		@retval		如果匹配失败，需要向红黑树的[右]子树继续匹配则返回正数，如:[1]
*/
typedef int (*rbTree_MatchRuleHandle_ptr)(const void* reference_node, const void* rbTree_node);
/**
*	@brief	红黑树资源释放处理句柄。当红黑树节点被释放时会调用该句柄。如果您保存的资源需要手动释放（如动态内存），请自定义该函数
*
*	@param
*		@param	void* resource		保存的规则
*
*	@retval
*		none
*/
typedef void (*rbTree_FreeRuleHandle_ptr)(void* resource);

/************************************************************************************************
*	红黑树创建与销毁
************************************************************************************************/

/**
*	@brief	创建一棵红黑树。注意,同一棵树的所有节点的索引类型必须相同，您如果选定了类型后就应该在这棵树使用同类型索引，否则可能引发严重错误。
*			其次,红黑树的资源类型建议相同，红黑树在进行节点删除操作时会调用自定义的清除规则对自定义资源进行回收，除非您能很好的解决不同类
*			型的资源泄露问题，否则不建议在同一棵红黑树中使用不同类型的资源。	
* 
*	@param
*		@param	rbTreeManager_t** rbTreeManager							红黑树管理器，它是用于管理一棵红黑树，同时它也是区分红黑树的唯一特征值
*		@param	unsigned int type_size									自定义索引类型的占用的空间大小。
*		@param  rbTree_MatchRuleHandle_ptr rbTree_MatchRuleHandle		该回调函数时此红黑树用于节点与节点比较的规则，您应该自定义一个合理的匹配函数，并且您不能传入 NULL
*		@param  rbTree_FreeRuleHandle_ptr rbTree_FreeRuleHandle			该回调函数会在出现节点删除的情况下使用，它用于您在释放节点保存的自定义资源，如果您的资源不需要手动释放，那么可以填写为 NULL
* 
*	@retval
*		@retval		创建成功返回 [0]
*		@retval		创建失败返回负值错误码，您可以使用 rbTree_ErrorCodePrint API打印错误语句
*/
int rbTree_Create(rbTreeManager_t** rbTreeManager,unsigned int type_size, rbTree_MatchRuleHandle_ptr rbTree_MatchRuleHandle, rbTree_FreeRuleHandle_ptr rbTree_FreeRuleHandle);
/**
*	@brief	释放整棵红黑树
*
*	@param
*		@param	rbTreeManager_t** rbTreeManager	 红黑树管理器，它是用于管理一棵红黑树，同时它也是区分红黑树的唯一特征值
*
*	@retval
*		none
*/
void rbTree_Free(rbTreeManager_t** rbTreeManager);
/************************************************************************************************
*	红黑树节点添加
************************************************************************************************/

/**
*	@brief	给指定红黑树添加一个节点
*
*	@param
*		@param	rbTreeManager_t* rbTreeManager	 红黑树管理器，它是用于管理一棵红黑树，同时它也是区分红黑树的唯一特征值
*		@param  const void* index				 为新节点添加一个索引，红黑树内部会保存该索引的副本，所以您可以使用局部变量
*		@param  void* resource					 新节点保存的资源，注意，红黑树内部不会保存该资源的副本，它会直接使用您传入的资源地址。
* 
*	@retval
*		@retval	添加成功则返回[0]
*		@retval 添加失败返回负值错误码，您可以使用 rbTree_ErrorCodePrint API打印错误语句
*/
int rbTree_AddNode(rbTreeManager_t* rbTreeManager, const void* index, void* resource);

/************************************************************************************************
*	红黑树节点删除
************************************************************************************************/

/**
*	@brief	给指定红黑树添加一个节点
*
*	@param
*		@param	rbTreeManager_t* rbTreeManager	 红黑树管理器，它是用于管理一棵红黑树，同时它也是区分红黑树的唯一特征值
*		@param  const void* index				 为新节点添加一个索引，红黑树内部会保存该索引的副本，所以您可以使用局部变量
*
*	@retval
*		@retval	none
*		@retval 您可以使用rbTree_IsErrorOccurred检查是否删除成功， 或使用 rbTree_ErrorCodePrint API打印错误语句
*/
void rbTree_DelNode(rbTreeManager_t* rbTreeManager, const void* index);

/************************************************************************************************
*	红黑树节点查找
************************************************************************************************/

/**
*	@brief	在指定红黑树中进行查找操作
*
*	@param
*		@param	rbTreeManager_t* rbTreeManager	 红黑树管理器，它是用于管理一棵红黑树，同时它也是区分红黑树的唯一特征值
*		@param  const void* index				 您的查找索引
*
*	@retval
*		@retval	查找成功则返回您在树中保存的资源
*		@retval 查找失败返回NULL 并且树中加更新错误码，您可以使用 rbTree_ErrorCodePrint API打印错误语句
*/
void* rbTree_Search(rbTreeManager_t* rbTreeManager, const void* index);

/************************************************************************************************
*	红黑树错误查询
************************************************************************************************/

/**
*	@brief	根据错误值打印该红黑树最近一次错误语句。红黑树内部会保存一个最近出现错误的错误码，该函数能根据错误码打印错误语句，当错误语句被打印一次后错误码会被清除。其次如果输入的[红黑树管理器]为NULL，则不会有任何输出
*
*	@param
*		@param	rbTreeManager_t* rbTreeManager		红黑树管理器
*	
*	@retval
*		none	
*/
void rbTree_ErrorCodePrint(rbTreeManager_t* rbTreeManager);
/**
*	@brief	用于检查当前红黑树是否出现了错误
*	@param
*		@param	rbTreeManager_t* rbTreeManager		红黑树管理器
*
*	@retval
*		@retval	如果没有出现过错误，返回0
*		@retval	如果出现过错误，则返回 负值错误值
*/
int rbTree_IsErrorOccurred(rbTreeManager_t* rbTreeManager);






/************************************************************************************************
*	红黑树测试
************************************************************************************************/
void print(rbTreeManager_t* rbTreeManager);


void printL(rbTreeManager_t* rbTreeManager);


#endif // !__RB_TREE_H__
