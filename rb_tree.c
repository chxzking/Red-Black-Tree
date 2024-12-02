#include "rb_tree.h"
#include <malloc.h>
#include <string.h>

#ifdef ENABLE_RBTREE_ERROR_CODE_PRINT
#include <stdio.h>
#endif

/******************************************************************************************************
*           作者：红笺画文
*           时间：2024年11月30日
*           版本：2.1
*
*           描述：一个红黑树模板库。
* 
*			版本修改说明：优化了内存布局，提高了CPU缓存命中率，提高了红黑树查询速度。
*
*			风格描述：我的设计理念是，使用尽可能少的内存与执行步骤，同时使用尽可能简洁的代码。每个函数只
*				      实现一种功能，且应尽量减少与外部数据的接触（例如全局变量），以最大程度避免多个实例
*                     之间的干扰。函数应该保持高度的封装性，防止用户直接干扰功能的实现。另外，API应该尽
*                     量少暴露实现细节，用户不应关注到功能的具体实现。
*
*******************************************************************************************************/




/******************************************************************************************************
*		私有区域说明：
*				红黑树的数据结构声明
*				红黑树私有函数声明
*
*
*
*
*
*
*
*******************************************************************************************************/
typedef enum RB_COLOR RB_COLOR;													//红黑树节点颜色
typedef struct rbTreeNode_t rbTreeNode_t;										//红黑树节点定义

#define		RB_TREE_NULL_PTR				((void*)0)				            //空指针定义

struct rbTreeManager_t {
	rbTreeNode_t* root;										//根节点
	unsigned int  index_typde_size;							//索引类型的数据长度
	rbTree_MatchRuleHandle_ptr rbTree_MatchRuleHandle;		//红黑树匹配规则句柄
	rbTree_FreeRuleHandle_ptr rbTree_FreeRuleHandle;		//红黑树资源释放句柄
	void* rbTree_buffer;									//红黑树内部的缓存区，它的大小等于一个索引数据的长度，用来作为节点交换的临时缓存区。
#ifdef ENABLE_RBTREE_ERROR_CODE_PRINT
	int errorCode;						//保存最近一次出错的错误码
#endif

};

//红黑树颜色
typedef enum RB_COLOR {
	RB_RED,				//红色
	RB_BLACK,			//黑色
	RB_ERR				//错误颜色
}RB_COLOR;

//红黑树节点
struct rbTreeNode_t {
	void* index;                 //红黑树索引
	void* resource;		         //资源信息
	struct rbTreeNode_t* right;		 //右孩子
	struct rbTreeNode_t* left;		 //左孩子
	struct rbTreeNode_t* parent;		 //父亲节点
	RB_COLOR color;				 //红黑树颜色
};

//错误码定义
#define NO_ERROR						0	//无错误
#define	RBTREE_ERRNO_OUT_OF_MEM			1	//内存不足
#define RBTREE_ERRNO_DUP_VAL			2	//目标值出现重复（可能的场景：添加的值已经被存在，那么会返回此错误）
#define	RBTREE_ERRNO_ARG_ERR			3	//参数错误
#define RBTREE_ERRNO_NODE_INEXIT		4	//节点不存在



//私有函数声明

RB_COLOR rbTreePrivate_GetNodeColor(rbTreeNode_t* rbTreeNode);
rbTreeNode_t* rbTreePrivate_CreateNewNode(rbTreeManager_t* rbTreeManager, const void* index, void* resource);
void rbTreePrivate_FreeNodeMem(rbTreeManager_t* rbTreeManager, rbTreeNode_t* rbTreeNode);
rbTreeNode_t* rbTreePrivate_Search(rbTreeManager_t* rbTreeManager, const void* index);
void  rbTreePrivate_ExchangeTwoNode(rbTreeManager_t* rbTreeManager,rbTreeNode_t* rbTreeNode1, rbTreeNode_t* rbTreeNode2);
void rbTreePrivate_LeftRotate(rbTreeNode_t** root, rbTreeNode_t* target_node);
void rbTreePrivate_RightRotate(rbTreeNode_t** root, rbTreeNode_t* target_node);
void rbTreePrivate_InsertAdjust(rbTreeNode_t** root, rbTreeNode_t* new_node);
void rbTreePrivate_DelNoneChildNodeAndAdjust(rbTreeManager_t* rbTreeManager, rbTreeNode_t* del_node);
void rbTreePrivate_DelRedNodeAndAdjust(rbTreeManager_t* rbTreeManager, rbTreeNode_t* del_node);
void rbTreePrivate_DelBlackNodeAndAdjust(rbTreeManager_t* rbTreeManager, rbTreeNode_t* del_node);

/******************************************************************************************************
*		私有区域说明：
*				红黑树私有函数的实现
*
*
*
*
*
*
*
*******************************************************************************************************/

/*
* @brief	获取指定节点的颜色
*
* @param	rbTreeNode_t* rbTreeNode	红黑树节点地址
*
* @retval	返回节点颜色
*/
RB_COLOR rbTreePrivate_GetNodeColor(rbTreeNode_t* rbTreeNode) {
	return rbTreeNode ? rbTreeNode->color : RB_BLACK;
}
/**
*	@brief 创建一个红黑树节点
*
*	@param	rbTreeManager_t* rbTreeManager	红黑树管理器
*	@param	void* index		为该节点添加索引
*	@param	void* resource	为该节点添加资源
*
*	@retval	创建成功返回 节点地址
*	@retval	创建失败返回 空
*/
rbTreeNode_t* rbTreePrivate_CreateNewNode(rbTreeManager_t* rbTreeManager, const void* index, void* resource) {
	//创建红黑树节点
	/*
	* 为了提高CPU缓存命中率，申请一整块连续的内存，并在逻辑上分为两段：
	*	1、第一段为数据段，用于存储节点信息
	*	2、第二段为索引段，用于保存索引信息
	* 
	*/
	rbTreeNode_t* node = (rbTreeNode_t*)malloc(sizeof(rbTreeNode_t) + rbTreeManager->index_typde_size);
	if (node == RB_TREE_NULL_PTR) {
		rbTreeManager->errorCode = -RBTREE_ERRNO_OUT_OF_MEM;
		return node;
	}


	//格式化内存块
	node->index = (void*)((char*)node + sizeof(rbTreeNode_t));

	//初始化节点值
	memcpy(node->index, index, rbTreeManager->index_typde_size);
	node->color = RB_RED;
	node->left = RB_TREE_NULL_PTR;
	node->parent = RB_TREE_NULL_PTR;
	node->right = RB_TREE_NULL_PTR;
	node->resource = resource;

	//返回
	return node;
}
/**
*	@brief 释放一个节点的空间
*
*	@param	rbTreeManager_t* rbTreeManager	红黑树管理器
*	@param	rbTreeNode_t* rbTreeNode
*
*	@retval	none
*/
void rbTreePrivate_FreeNodeMem(rbTreeManager_t* rbTreeManager, rbTreeNode_t* rbTreeNode) {
	//释放节点资源
	if (rbTreeManager->rbTree_FreeRuleHandle != RB_TREE_NULL_PTR) {
		rbTreeManager->rbTree_FreeRuleHandle(rbTreeNode->resource);
	}
	//释放节点空间
	free(rbTreeNode);
	return;
}
/**
*	@brief 在红黑树中搜索指定索引
*
*	@param	rbTreeManager_t* rbTreeManager	红黑树管理器
*	@param	void* index						索引
*
*	@retval		成功返回 节点地址
*	@retval		失败返回 空
*/
rbTreeNode_t* rbTreePrivate_Search(rbTreeManager_t* rbTreeManager, const void* index) {
	rbTreeNode_t* probe = rbTreeManager->root;
	int match_ret;
	//遍历红黑树
	while (probe != RB_TREE_NULL_PTR) {
		match_ret = rbTreeManager->rbTree_MatchRuleHandle(index, probe->index);
		//如果匹配成功
		if (match_ret == 0) {
			return probe;
		}
		//当前节点比参考节点大，请求向左子树遍历
		else if (match_ret > 0) {
			probe = probe->left;
		}
		//当前节点比参考节点小，请求向右子树遍历
		else {
			probe = probe->right;
		}
	}
	//没有找到目标搜索结果
	return RB_TREE_NULL_PTR;
}
/**
*	@brief 交换两个节点，注意：如果两个节点有任意一个节点为空，那么便不会发生交换
*
*	@param	rbTreeManager_t* rbTreeManager	红黑树管理器
*	@param	rbTreeNode_t* rbTreeNode1	节点1
*	@param	rbTreeNode_t* rbTreeNode2	节点2
*
*	@retval		none
*/
void  rbTreePrivate_ExchangeTwoNode(rbTreeManager_t* rbTreeManager,rbTreeNode_t* rbTreeNode1, rbTreeNode_t* rbTreeNode2) {
	if (rbTreeNode1 == RB_TREE_NULL_PTR || rbTreeNode2 == RB_TREE_NULL_PTR)	return;

	
	//交换两个节点的索引
	memcpy(rbTreeManager->rbTree_buffer, rbTreeNode1->index, rbTreeManager->index_typde_size);
	memcpy(rbTreeNode1->index, rbTreeNode2->index, rbTreeManager->index_typde_size);
	memcpy(rbTreeNode2->index, rbTreeManager->rbTree_buffer, rbTreeManager->index_typde_size);

	//交换两个节点的资源
	void* temp = RB_TREE_NULL_PTR;
	temp = rbTreeNode1->resource;
	rbTreeNode1->resource = rbTreeNode2->resource;
	rbTreeNode2->resource = temp;

	return;
}
/**
*	@brief 将一个节点左旋
*			(图像描述如下：)
*				[] --> target_node
*			   /  \
*			  []   []
*
*	@param	rbTreeNode_t** root			红黑树的根节点
*	@param	rbTreeNode_t* target_node	当前旋转体的根节点，也就是被旋转的节点
*
*	@retval		none
*/
void rbTreePrivate_LeftRotate(rbTreeNode_t** root, rbTreeNode_t* target_node) {
	rbTreeNode_t* new_node = target_node->right;
	target_node->right = new_node->left;

	//
	if (new_node->left != RB_TREE_NULL_PTR) {
		new_node->left->parent = target_node;
	}

	//更新新的局部子树的根节点的父亲节点
	new_node->parent = target_node->parent;
	if (target_node->parent == RB_TREE_NULL_PTR) {
		*root = new_node;
	}
	else if (target_node == target_node->parent->left) {
		target_node->parent->left = new_node;
	}
	else {
		target_node->parent->right = new_node;
	}
	//
	target_node->parent = new_node;
	new_node->left = target_node;

	return;
}
/**
*	@brief 将一个节点右旋
*			(图像描述如下：)
*				[] --> target_node
*			   /  \
*			  []   []
*
*	@param	rbTreeNode_t** root			红黑树的根节点
*	@param	rbTreeNode_t* target_node	当前旋转体的根节点，也就是被旋转的节点
*
*	@retval		none
*/
void rbTreePrivate_RightRotate(rbTreeNode_t** root, rbTreeNode_t* target_node) {
	rbTreeNode_t* new_node = target_node->left;
	target_node->left = new_node->right;
	if (new_node->right != RB_TREE_NULL_PTR) {
		new_node->right->parent = target_node;
	}
	//
	new_node->parent = target_node->parent;
	if (target_node->parent == RB_TREE_NULL_PTR) {
		*root = new_node;
	}
	else if (target_node == target_node->parent->right) {
		target_node->parent->right = new_node;
	}
	else {
		target_node->parent->left = new_node;
	}
	//
	new_node->right = target_node;
	target_node->parent = new_node;

	return;
}
/**
*	@brief 对一个添加了新节点的红黑树进行调整
*
*	@param	rbTreeNode_t** root			红黑树的根节点
*	@param	rbTreeNode_t* new_node		新添加的节点
*
*	@retval		none
*/
void rbTreePrivate_InsertAdjust(rbTreeNode_t** root, rbTreeNode_t* new_node) {
	/*添加节点存在三种情况，分别是
	*	1、插入的节点是根节点
	*	2、插入节点的叔叔节点是红色
	*	3、插入节点的叔叔节点是黑色
	*/

	//定义叔叔节点
	rbTreeNode_t* uncle = RB_TREE_NULL_PTR;

	//循环条件：当前节点不为根节点，并且父亲节点的颜色为红色
	while (new_node->parent && new_node->parent->color == RB_RED) {
		//当父亲节点是祖先节点的左节点时
		if (new_node->parent == new_node->parent->parent->left) {
			// 获取叔叔节点
			uncle = new_node->parent->parent->right;
			//叔叔节点为红色
			if (uncle && uncle->color == RB_RED) {
				//父亲节点变黑
				new_node->parent->color = RB_BLACK;
				//叔叔节点变黑
				uncle->color = RB_BLACK;
				//祖先节点变成红色
				new_node->parent->parent->color = RB_RED;
				//将当前节点提升到祖先节点的位置
				new_node = new_node->parent->parent;
			}
			//叔叔节点为黑色
			else {
				//检查旋转类型
				if (new_node == new_node->parent->right) {
					//如果是右节点插入，是LR型，先将子树左旋转化为LL型
					new_node = new_node->parent;
					rbTreePrivate_LeftRotate(root, new_node);
				}
				// LL型处理
				new_node->parent->color = RB_BLACK;
				new_node->parent->parent->color = RB_RED;
				rbTreePrivate_RightRotate(root, new_node->parent->parent);
			}
		}
		//当父亲节点是祖先节点的右节点时
		else {
			uncle = new_node->parent->parent->left; // 叔叔节点
			// 叔叔是红色
			if (uncle && uncle->color == RB_RED) {
				//父亲节点变黑
				new_node->parent->color = RB_BLACK;
				//叔叔节点变黑
				uncle->color = RB_BLACK;
				//祖先节点变成红色
				new_node->parent->parent->color = RB_RED;
				//将当前节点提升到祖先节点的位置
				new_node = new_node->parent->parent;
			}
			//叔叔节点为黑色
			else {
				//检查旋转类型
				if (new_node == new_node->parent->left) {
					//如果是左节点插入，是RL型，先将子树左旋转化为RR型
					new_node = new_node->parent;
					rbTreePrivate_RightRotate(root, new_node);
				}
				// RR型处理	
				new_node->parent->color = RB_BLACK;
				new_node->parent->parent->color = RB_RED;
				rbTreePrivate_LeftRotate(root, new_node->parent->parent);
			}
		}
	}
	(*root)->color = RB_BLACK; // 根节点始终为黑色
	return;
}
/**
*	@brief 删除一个没有孩子的节点
*
*	@param	rbTreeManager_t* rbTreeManager		红黑树管理器
*	@param	rbTreeNode_t* del_node				被删除的节点
*
*	@retval		none
*/
void rbTreePrivate_DelNoneChildNodeAndAdjust(rbTreeManager_t* rbTreeManager, rbTreeNode_t* del_node) {
	//如果被删除的节点是根节点
	if (del_node->parent == RB_TREE_NULL_PTR) {
		rbTreePrivate_FreeNodeMem(rbTreeManager, del_node);
		rbTreeManager->root = RB_TREE_NULL_PTR;
		return;
	}

	//被删除的节点是黑色
	if (del_node->color == RB_BLACK) {
		rbTreePrivate_DelBlackNodeAndAdjust(rbTreeManager, del_node);
	}
	//被删除的节点是红色
	else {
		rbTreePrivate_DelRedNodeAndAdjust(rbTreeManager, del_node);
	}
	return;
}
/**
*	@brief 删除一个红色的节点
*
*	@param	rbTreeManager_t* rbTreeManager		红黑树管理器
*	@param	rbTreeNode_t* del_node				被删除的节点
*
*	@retval		none
*/
void rbTreePrivate_DelRedNodeAndAdjust(rbTreeManager_t* rbTreeManager, rbTreeNode_t* del_node) {
	//如果被删除节点是父亲节点的左孩子
	if (del_node == del_node->parent->left) {
		del_node->parent->left = RB_TREE_NULL_PTR;
	}
	//如果被删除节点是父亲节点的右孩子
	else {
		del_node->parent->right = RB_TREE_NULL_PTR;
	}
	rbTreePrivate_FreeNodeMem(rbTreeManager, del_node);
	return;
}
/**
*	@brief 删除一个黑色的节点
*
*	@param	rbTreeManager_t* rbTreeManager		红黑树管理器
*	@param	rbTreeNode_t* target				被删除的节点
*
*	@retval		none
*/
void rbTreePrivate_DelBlackNodeAndAdjust(rbTreeManager_t* rbTreeManager, rbTreeNode_t* target) {
	/*删除黑色节点的情况主要分为两个大类：
	*	1、被删除节点的叔叔节点是红色的
	*	2、被删除节点的叔叔节点是黑色的
	*	3、不存在没有叔叔节点的情况
	*/

	/*	对于首次传入的target：
	*
	*	1、target一定不可能为NULL
	*	2、target一定不可能是根节点，根节点已经被过滤
	*/

	//获取叔叔节点以及删除目标节点
	rbTreeNode_t* uncle = RB_TREE_NULL_PTR;
	if (target == target->parent->left) {
		uncle = target->parent->right;
		target->parent->left = RB_TREE_NULL_PTR;
	}
	else {
		uncle = target->parent->left;
		target->parent->right = RB_TREE_NULL_PTR;
	}
	rbTreePrivate_FreeNodeMem(rbTreeManager, target);//删除节点
	//如果叔叔节点为空，意味着当前红黑树只剩下两个节点，删除一个后只剩下了一个，那么便无需在调整了。
	if (uncle == RB_TREE_NULL_PTR) {
		return;
	}
READJUST_FLAG:	//重复调整标志位

	//树的调整
	//处理情况1：被删除节点的叔叔节点是红色的
	if (uncle->color == RB_RED) {
		//预处理，将节点颜色进行更新
		uncle->parent->color = RB_RED;
		uncle->color = RB_BLACK;

		target = uncle->parent;//注意这个地方的target作为旋转节点的根节点。

		//分左右子树方向进行处理
		//如果剩余的子树是左子树，那么进行右旋处理
		if (target->left == uncle) {
			for (int i = 0; i < 2; i++) {
				rbTreePrivate_RightRotate(&rbTreeManager->root, target);
			}
		}
		//如果剩余的子树是右子树，那么进行左旋处理
		else {
			for (int i = 0; i < 2; i++) {
				rbTreePrivate_LeftRotate(&rbTreeManager->root, target);
			}
		}
		return;
	}
	//处理情况2：被删除节点的叔叔节点是黑色的
	else {
		/*	分类讨论：叔叔节点是否存在子节点
		*
		*	1、存在子节点，且子节点是红色，那么存在多余的节点可以转变为黑色节点传递到被删除的子树上。
		*
		*	2、不存在子节点
		*		2.1、父亲节点是红色
		*		2.2、父亲节点是黑色
		*/

		//判断当前叔叔节点所在的子树： 左子树
		if (uncle->parent->left == uncle) {

			//判断叔叔节点是否存在红色左孩子
			if (uncle->left && uncle->left->color == RB_RED) {
				//颜色调整
				uncle->left->color = RB_BLACK;
				uncle->color = uncle->parent->color;
				uncle->parent->color = RB_BLACK;
				//旋转调整
				rbTreePrivate_RightRotate(&rbTreeManager->root, uncle->parent);
				return;
			}
			//左孩子不存在，则判断叔叔节点是否存在红色右孩子
			else if (uncle->right && uncle->right->color == RB_RED) {
				//转化为LL型
				rbTreePrivate_LeftRotate(&rbTreeManager->root, uncle);
				//调整颜色
				uncle->parent->color = uncle->parent->parent->color;
				uncle->parent->parent->color = RB_BLACK;
				//旋转调整
				rbTreePrivate_RightRotate(&rbTreeManager->root, uncle->parent);
				return;
			}
		}
		//当前叔叔节点所在的子树： 右子树
		else {
			//判断叔叔节点是否存在红色右孩子
			if (uncle->right && uncle->right->color == RB_RED) {
				//颜色调整
				uncle->right->color = RB_BLACK;
				uncle->color = uncle->parent->color;
				uncle->parent->color = RB_BLACK;
				//旋转调整
				rbTreePrivate_LeftRotate(&rbTreeManager->root, uncle->parent);
				return;
			}
			//右孩子不存在，则判断叔叔节点是否存在左孩子
			else if (uncle->left && uncle->left->color == RB_RED) {
				//转化为RR型
				rbTreePrivate_RightRotate(&rbTreeManager->root, uncle);
				//颜色调整
				uncle->parent->color = uncle->parent->parent->color;
				uncle->parent->parent->color = RB_BLACK;
				//旋转调整
				rbTreePrivate_LeftRotate(&rbTreeManager->root, uncle->parent);
				return;
			}
		}

		//执行到此处意味着叔叔节点的孩子节点均不存在，则判断父亲节点是否为黑色
		if (uncle->parent->color == RB_BLACK) {											//////////////////////////
			//叔叔节点变色,然后寻找更高层次的树，帮助这个子树进行调整
			uncle->color = RB_RED;
			//如果父亲节点是根节点那么就不需要处理，如果不是根节点,那么需要重置uncle
			if (uncle->parent->parent) {
				//判断子树的根节点是更高层的树的左孩子还是右孩子
				//左孩子
				if (uncle->parent == uncle->parent->parent->left) {
					uncle = uncle->parent->parent->right;
				}
				//右孩子
				else {
					uncle = uncle->parent->parent->left;
				}
				goto READJUST_FLAG;//进入下一个轮回
			}
		}
		//此类情况为父亲为红色，并且叔叔节点没有任何子节点,这类情况最简单，不需要任何调整，只需要改变颜色即可
		else {
			uncle->color = RB_RED;
			uncle->parent->color = RB_BLACK;
		}
		return;
	}
}




/******************************************************************************************************
*		API区域说明：
*				红黑树API函数的实现
*
*
*
*
*
*
*
*******************************************************************************************************/


int rbTree_Create(rbTreeManager_t** rbTreeManager, unsigned int type_size, rbTree_MatchRuleHandle_ptr rbTree_MatchRuleHandle, rbTree_FreeRuleHandle_ptr rbTree_FreeRuleHandle) {
	//参数合法性检查
	if (rbTreeManager == RB_TREE_NULL_PTR || type_size == 0 || rbTree_MatchRuleHandle == RB_TREE_NULL_PTR)	return -RBTREE_ERRNO_ARG_ERR;

	//创建红黑树树管理器
	*rbTreeManager = (rbTreeManager_t*)malloc(sizeof(rbTreeManager_t));
	if (*rbTreeManager == RB_TREE_NULL_PTR) {
		return -RBTREE_ERRNO_OUT_OF_MEM;
	}
	//*rbTreeManager
	(*rbTreeManager)->rbTree_buffer = malloc(sizeof(type_size));
	if ((*rbTreeManager)->rbTree_buffer == RB_TREE_NULL_PTR) {
		free(*rbTreeManager);
		return -RBTREE_ERRNO_OUT_OF_MEM;
	}

	//初始化
	(*rbTreeManager)->root = RB_TREE_NULL_PTR;
	(*rbTreeManager)->index_typde_size = type_size;
	(*rbTreeManager)->rbTree_MatchRuleHandle = rbTree_MatchRuleHandle;
	(*rbTreeManager)->rbTree_FreeRuleHandle = rbTree_FreeRuleHandle;
#ifdef ENABLE_RBTREE_ERROR_CODE_PRINT
	(*rbTreeManager)->errorCode = NO_ERROR;
#endif
	return NO_ERROR;
}

void rbTree_Free(rbTreeManager_t** rbTreeManager) {
	if (rbTreeManager == RB_TREE_NULL_PTR)	return;

	//逐个释放红黑树节点
	*rbTreeManager = NULL;
}

int rbTree_AddNode(rbTreeManager_t* rbTreeManager, const void* index, void* resource) {
	//参数检查
	if (rbTreeManager == RB_TREE_NULL_PTR)	return -RBTREE_ERRNO_ARG_ERR;
	if (index == RB_TREE_NULL_PTR) {
		rbTreeManager->errorCode = -RBTREE_ERRNO_ARG_ERR;
		return -RBTREE_ERRNO_ARG_ERR;
	}

	//寻找合适的地方进行插入
	rbTreeNode_t* slow = RB_TREE_NULL_PTR;
	rbTreeNode_t* fast = rbTreeManager->root;
	int match_ret;
	while (fast != RB_TREE_NULL_PTR) {
		slow = fast;
		match_ret = rbTreeManager->rbTree_MatchRuleHandle(fast->index, index);//匹配含义：将输入的节点依次与树中的节点索引进行比较
		//匹配成功
		if (match_ret == 0) {
			rbTreeManager->errorCode = -RBTREE_ERRNO_DUP_VAL;
			return -RBTREE_ERRNO_DUP_VAL;
		}
		//请求向左子树遍历
		else if (match_ret < 0) {
			fast = fast->left;
		}
		//请求向右子树遍历
		else {
			fast = fast->right;
		}
	}

	//创建节点
	rbTreeNode_t* new_node = rbTreePrivate_CreateNewNode(rbTreeManager, index, resource);
	if (new_node == RB_TREE_NULL_PTR)	return -RBTREE_ERRNO_OUT_OF_MEM;

	//将节点添加到树中
	new_node->parent = slow;
	if (slow == RB_TREE_NULL_PTR) {
		rbTreeManager->root = new_node;
	}
	else {
		match_ret = rbTreeManager->rbTree_MatchRuleHandle(slow->index, new_node->index);//匹配含义：将新添加的节点与父亲节点进行比较，判断自己为左孩子还是右孩子
		if (match_ret > 0) {
			slow->right = new_node;
		}
		else {
			slow->left = new_node;
		}
	}

	//调整红黑树
	rbTreePrivate_InsertAdjust(&rbTreeManager->root, new_node);
	return NO_ERROR;
}

void rbTree_DelNode(rbTreeManager_t* rbTreeManager, const void* index) {
	//参数合法性检查
	if (rbTreeManager == RB_TREE_NULL_PTR)	return;
	if (index == RB_TREE_NULL_PTR) {
		rbTreeManager->errorCode = -RBTREE_ERRNO_ARG_ERR;
		return;
	}

	//查找目标索引
	rbTreeNode_t* target = rbTreePrivate_Search(rbTreeManager, index);
	if (target == RB_TREE_NULL_PTR) {
		rbTreeManager->errorCode = -RBTREE_ERRNO_NODE_INEXIT;
		return;
	}

	/*
	* 按照度的数量进行分类处理
	*/

	//如果左右孩子都存在（单独拿出来，因为它不处理节点，只交换两个节点）(度为2的情况)
	if (target->left && target->right) {
		//获取右子树最小节点
		rbTreeNode_t* min_right_tree = target->right;
		while (min_right_tree->left != RB_TREE_NULL_PTR) {
			min_right_tree = min_right_tree->left;
		}
		//交换两个节点
		rbTreePrivate_ExchangeTwoNode(rbTreeManager,target, min_right_tree);
		target = min_right_tree;
	}

	//只存在左孩子的情况，将孩子父亲节点相连接(度为1的情况)
	if (target->left) {
		target->left->parent = target->parent;
		target->left->color = RB_BLACK;
		//父亲节点是根节点
		if (target->parent == RB_TREE_NULL_PTR) {
			rbTreeManager->root = target->left;
		}
		//被删除节点是父亲节点的左孩子
		else if (target == target->parent->left) {
			target->parent->left = target->left;
		}
		//被删除节点是父亲节点的右孩子
		else if (target == target->parent->right) {
			target->parent->right = target->left;
		}
		rbTreePrivate_FreeNodeMem(rbTreeManager, target);//节点删除
		return;
	}
	//只存在右孩子的情况，将孩子父亲节点相连接(度为1的情况)
	else if (target->right) {
		target->right->parent = target->parent;
		target->right->color = RB_BLACK;
		//父亲节点是根节点
		if (target->parent == RB_TREE_NULL_PTR) {
			rbTreeManager->root = target->right;
		}
		//被删除节点是父亲节点的左孩子
		else if (target == target->parent->left) {
			target->parent->left = target->right;
		}
		//被删除节点是父亲节点的右孩子
		else if (target == target->parent->right) {
			target->parent->right = target->right;
		}
		rbTreePrivate_FreeNodeMem(rbTreeManager, target);//删除节点
		return;
	}
	//不存在孩子(度为0的情况)
	else {
		rbTreePrivate_DelNoneChildNodeAndAdjust(rbTreeManager, target);
		return;
	}
}

void* rbTree_Search(rbTreeManager_t* rbTreeManager, const void* index) {
	//参数检查
	if (rbTreeManager == RB_TREE_NULL_PTR)	return RB_TREE_NULL_PTR;
	if (index == RB_TREE_NULL_PTR) {
		rbTreeManager->errorCode = -RBTREE_ERRNO_ARG_ERR;
		return RB_TREE_NULL_PTR;
	}
	//树的遍历
	rbTreeNode_t* probe = rbTreePrivate_Search(rbTreeManager, index);
	//返回搜索结果
	return probe ? probe->resource : RB_TREE_NULL_PTR;
}

void rbTree_ErrorCodePrint(rbTreeManager_t* rbTreeManager) {
#ifdef ENABLE_RBTREE_ERROR_CODE_PRINT
	if (rbTreeManager == NULL) {
		printf("rbTreeManager_t非法\n");
		return;
	}
	//错误判断
	switch (rbTreeManager->errorCode) {
	case NO_ERROR:
		printf("没有发生错误\n");
		break;
	case -RBTREE_ERRNO_OUT_OF_MEM:
		printf("申请动态内存失败\n");
		break;
	case -RBTREE_ERRNO_DUP_VAL:
		printf("出现重复值\n");
		break;
	case -RBTREE_ERRNO_ARG_ERR:
		printf("参数错误\n");
		break;
	case -RBTREE_ERRNO_NODE_INEXIT:
		printf("节点不存在\n");
		break;
	}
	//清除错误码
	rbTreeManager->errorCode = NO_ERROR;
#endif
	return;
}

int rbTree_IsErrorOccurred(rbTreeManager_t* rbTreeManager) {
#ifdef ENABLE_RBTREE_ERROR_CODE_PRINT
	if (rbTreeManager->errorCode != NO_ERROR) {
		return rbTreeManager->errorCode;
	}
#endif
	return NO_ERROR;
}



/******************************************************************************************************
*		API区域说明：
*				测试专用API
*
*
*
*
*
*
*
*******************************************************************************************************/

void func(rbTreeNode_t* root, rbTree_MatchRuleHandle_ptr rbTree_MatchRuleHandle) {
	if (root == NULL)	return;

	func(root->left, rbTree_MatchRuleHandle);
	printf("%02d ", *((int*)root->resource));
	func(root->right, rbTree_MatchRuleHandle);
}
void print(rbTreeManager_t* rbTreeManager) {
	func(rbTreeManager->root, rbTreeManager->rbTree_MatchRuleHandle);
}


#include <stdbool.h>


void func(rbTreeNode_t* root, rbTree_MatchRuleHandle_ptr rbTree_MatchRuleHandle) {
	if (root == NULL)	return;

	func(root->left, rbTree_MatchRuleHandle);
	printf("%02d ", *((int*)root->resource));
	func(root->right, rbTree_MatchRuleHandle);
}
void print(rbTreeManager_t* rbTreeManager) {
	func(rbTreeManager->root, rbTreeManager->rbTree_MatchRuleHandle);
}


// 定义队列节点
typedef struct QueueNode {
	rbTreeNode_t* treeNode;
	struct QueueNode* next;
} QueueNode_t;

// 定义队列
typedef struct Queue {
	QueueNode_t* front;
	QueueNode_t* rear;
} Queue_t;

// 初始化队列
void initQueue(Queue_t* q) {
	q->front = q->rear = NULL;
}

// 入队
void enqueue(Queue_t* q, rbTreeNode_t* node) {
	QueueNode_t* newQueueNode = (QueueNode_t*)malloc(sizeof(QueueNode_t));
	newQueueNode->treeNode = node;
	newQueueNode->next = NULL;
	if (q->rear) {
		q->rear->next = newQueueNode;
	}
	else {
		q->front = newQueueNode;
	}
	q->rear = newQueueNode;
}

// 出队
rbTreeNode_t* dequeue(Queue_t* q) {
	if (q->front == NULL) return NULL;
	QueueNode_t* temp = q->front;
	rbTreeNode_t* treeNode = temp->treeNode;
	q->front = q->front->next;
	if (q->front == NULL) q->rear = NULL;
	free(temp);
	return treeNode;
}

// 判断队列是否为空
bool isQueueEmpty(Queue_t* q) {
	return q->front == NULL;
}

// 层次遍历打印红黑树
void printLevelOrder(rbTreeNode_t* root) {
	if (!root) {
		printf("NULL\n");
		return;
	}

	Queue_t queue;
	initQueue(&queue);
	enqueue(&queue, root);

	while (!isQueueEmpty(&queue)) {
		int nodeCount = 0;
		QueueNode_t* tempQueueNode = queue.front;
		while (tempQueueNode) {
			nodeCount++;
			tempQueueNode = tempQueueNode->next;
		}

		while (nodeCount > 0) {
			rbTreeNode_t* node = dequeue(&queue);
			if (node) {
				printf("%d", *((int*)node->resource));
				if (node->color == RB_BLACK) {
					printf("(黑) ");
				}
				else {
					printf("(红) ");
				}
				enqueue(&queue, node->left);
				enqueue(&queue, node->right);
			}
			else {
				printf("NULL ");
			}
			nodeCount--;
		}
		printf("\n");
	}
}


void printL(rbTreeManager_t* rbTreeManager) {
	printLevelOrder(rbTreeManager->root);
}
