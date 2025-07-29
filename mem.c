#include<stdatomic.h>
#include<stdint.h>
#include<stdlib.h>
#include<string.h>

//今回は4096人ようのメモリプールを作成する。

//仮のuser_info_t
typedef struct {
	int sock;
	uint8_t flag[2];
	char user_uuid[36];
	char user_email[255];//RFC5321、まぁ、今回は関係ないけど
}user_info_t;


typedef struct __attribute__((packed,aligned(16))){//AIの案(__attributeは私が追加した)
	uint64_t flag;//ここは一般的にはvoid *らしい。（俺が工夫したところ）
	uint64_t version;
}TaggedValue;


typedef union{//ここもAIのコード
	TaggedValue s;
	unsigned __int128 u128;
}TaggedValueUnion;


typedef struct{
	uint8_t flag[2];
}flag_t;//return の時にこっちのほうが使い易い。


typedef struct {
	_Atomic unsigned __int128 flag1;
	_Atomic unsigned __int128 flag2[64];
	user_info_t pool[64][64];//今回は2段階までにするけど、理論上どれだけ増やしても大丈夫だし、これならアクセス速度も一定
}memory_pool_t;


memory_pool_t user_pool;

user_info_t *allocate_memory(memory_pool_t *pool);//メモリを借りる為の関数
void release_memory(memory_pool_t *pool,user_info_t *flag);//メモリを返す関数
void init_memory_pool(memory_pool_t *pool);//ただの0埋め


user_info_t *allocate_memory(memory_pool_t *pool){
	uint8_t flag[2];
	TaggedValueUnion expected_union;
	TaggedValueUnion desired_union;
allocate_memory_up_bit_flag1:
	expected_union.u128=atomic_load_explicit(&pool->flag1,memory_order_acquire);
	if(__builtin_expect(expected_union.s.flag==UINT64_MAX,0)){
		return NULL;
	}
	desired_union.u128=expected_union.u128;
	desired_union.s.version++;
	flag[0]= (uint8_t)__builtin_ctzll(~expected_union.s.flag);
	desired_union.s.flag|=1ULL<<flag[0];
	if(__builtin_expect(!atomic_compare_exchange_strong_explicit(&pool->flag1,&expected_union.u128,desired_union.u128,memory_order_release,memory_order_acquire),0))goto allocate_memory_up_bit_flag1;
	//flag2
allocate_memory_up_bit_flag2:
	expected_union.u128=atomic_load_explicit(&pool->flag2[flag[0]],memory_order_acquire);
	if(__builtin_expect(expected_union.s.flag==UINT64_MAX,0))goto allocate_memory_up_bit_flag1;
	desired_union.u128=expected_union.u128;
	desired_union.s.version++;
	flag[1]=(uint8_t)__builtin_ctzll(~expected_union.s.flag);
	desired_union.s.flag|=1ULL<<flag[1];
	if(__builtin_expect(!atomic_compare_exchange_strong_explicit(&pool->flag2[flag[0]],&expected_union.u128,desired_union.u128,memory_order_release,memory_order_acquire),0))goto allocate_memory_up_bit_flag2;
	return &user_pool.pool[flag[0]][flag[1]];
}

void release_memory(memory_pool_t *pool,user_info_t *info){
	TaggedValueUnion expected_union;//ここAIのねーみんぐ
	TaggedValueUnion desired_union;//ここAIのネーミング
	//一旦メモリからロード
release_memory_down_bit_flag_flag2:
	expected_union.u128=atomic_load_explicit(&pool->flag2[info->flag[0]],memory_order_acquire);
	desired_union.u128=expected_union.u128;//コピー
	desired_union.s.version++;
	desired_union.s.flag &=~(1ULL<<info->flag[1]);
	if(__builtin_expect(!atomic_compare_exchange_strong_explicit(&pool->flag2[info->flag[0]],&expected_union.u128,desired_union.u128,memory_order_release,memory_order_acquire),0))goto release_memory_down_bit_flag_flag2;
	//今度はflag1を設定する。
	//ifを入れるとヒットしなかったら嫌だから。
release_memory_down_bit_flag_flag1:
	expected_union.u128=atomic_load_explicit(&pool->flag1,memory_order_acquire);
	desired_union.u128=expected_union.u128;
	desired_union.s.version++;
	desired_union.s.flag &=~(1ULL<<info->flag[0]);
	if(__builtin_expect(!atomic_compare_exchange_strong_explicit(&pool->flag1,&expected_union.u128,desired_union.u128,memory_order_release,memory_order_acquire),0))goto release_memory_down_bit_flag_flag1;
	return;
}


//本来はサーバープログラミングの時にはmainで初期化すればいいけど一応入れとくね／
void init_memory_pool(memory_pool_t *pool){
	memset(pool,0,sizeof(memory_pool_t));
	return;
}
