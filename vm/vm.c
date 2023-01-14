/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"

/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
void
vm_init (void) {
	vm_anon_init ();
	vm_file_init ();
#ifdef EFILESYS  /* For project 4 */
	pagecache_init ();
#endif
	register_inspect_intr ();
	/* DO NOT MODIFY UPPER LINES. */
	/* TODO: Your code goes here. */
}

/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */
enum vm_type
page_get_type (struct page *page) {
	int ty = VM_TYPE (page->operations->type);
	switch (ty) {
		case VM_UNINIT:
			return VM_TYPE (page->uninit.type);
		default:
			return ty;
	}
}

/* Helpers */
static struct frame *vm_get_victim (void);
static bool vm_do_claim_page (struct page *page);
static struct frame *vm_evict_frame (void);

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
/* 커널이 새로운 페이지 요청받으면 호출
	각 타입에 맞게 uninit_new로 aux와 함께 page 저장
	spt에 추가 */
bool vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable, vm_initializer *init, void *aux) {

	ASSERT (VM_TYPE(type) != VM_UNINIT);

	struct supplemental_page_table *spt = &thread_current ()->spt;

	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page (spt, upage) == NULL) {
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */
		struct page *page = (struct page*)calloc(1, sizeof(struct page));
		bool (*initializer)(struct page *, enum vm_type, void *kva);

		switch(VM_TYPE(type))
		{
			case VM_ANON: 
				initializer = anon_initializer;
				uninit_new(page, upage, init, type, aux, initializer);
				break;
			case VM_FILE:
				initializer = file_backed_initializer;
				uninit_new(page, upage, init, type, aux, initializer);
				break;
		}
		page->writable = writable;
		// 스택인 경우 구분
		if (type == VM_STACK)	// anon과 비교되야 하므로 VM_TYPE() 없이
			page->is_stack = true;
		
		/* TODO: Insert the page into the spt. */
		if(!spt_insert_page(spt, page)){
			return false;
		}
		return true;
	}
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct supplemental_page_table *spt UNUSED, void *va UNUSED) {
	struct page *page = NULL;

	/* TODO: Fill this function. */
	page = (struct page*)calloc(1, sizeof(struct page));
	page->va = pg_round_down(va);
	struct hash_elem *tmp_elem = hash_find(&spt->sup_hash, &page->hash_e);
	free(page);

	return tmp_elem != NULL ? hash_entry(tmp_elem, struct page, hash_e) : NULL;
}

/* Insert PAGE into spt with validation. */
bool spt_insert_page (struct supplemental_page_table *spt UNUSED, struct page *page UNUSED) {
	int succ = false;
	/* TODO: Fill this function. */

	struct hash_elem *check_page_elem = hash_find(&spt->sup_hash, &page->hash_e);

	// 에러 체크 - check_page_elem이 NULL인 경우
	if(check_page_elem != NULL)
		// false 반환
		return succ;
	// check_page_elem가 비어있지 않은 경우
	else{
		hash_insert(&spt->sup_hash, &page->hash_e);
		succ = true;
	}
	return succ;
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	vm_dealloc_page (page);
	return true;
}

/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim (void) {
	struct frame *victim = NULL;
	 /* TODO: The policy for eviction is up to you. */

	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame (void) {
	struct frame *victim UNUSED = vm_get_victim ();
	/* TODO: swap out the victim and return the evicted frame. */

	return NULL;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
static struct frame *
vm_get_frame (void) {
	struct frame *frame = NULL;
	/* TODO: Fill this function. */
	frame = (struct frame *)calloc(1, sizeof(struct frame));	// TODO: 할당해제
	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);
	frame->kva = palloc_get_page(PAL_USER);
	frame->page == NULL;

	if(frame->kva == NULL){
		PANIC("todo");	// swap in 해줘야함 나중에
	}
	else{
		return frame;
	}
}

/* Growing the stack. */
static void
vm_stack_growth (void *addr UNUSED) {
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* Return true on success */
/* page fault 에서 호출 */
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED, bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
	struct supplemental_page_table *spt UNUSED = &thread_current ()->spt;
	struct page *page = NULL;
	/* TODO: Validate the fault */
	/* TODO: Your code goes here */
	// 인자로 넘겨받은 에러들로 에러별 핸들러를 구현
	if(is_kernel_vaddr(addr)){
		return false;
	}
	// 현재 페이지가 없는 경우
	if (not_present) {
		// 보조 페이지 테이블에서 주소에 맞는 페이지 가져오기
        page = spt_find_page(spt, addr);

		// 가져온 페이지가 NULL인 경우
        if (page == NULL)
			// false 리턴
            return false;

		// 페이지에 프레임을 할당하고 mmu 설정
        if (vm_do_claim_page (page) == false)
			// 실패한 경우 false 리턴
            return false;
    }

	return true;
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void
vm_dealloc_page (struct page *page) {
	destroy (page);
	free (page);
}

/* Claim the page that allocate on VA. */
/* va로 spt 에서 page 찾고, do_claim으로 frame 할당받고
	pml4에 연결 */
bool
vm_claim_page (void *va UNUSED) {
	struct page *page = NULL;
	/* TODO: Fill this function */
	struct thread *t = thread_current();
	page = spt_find_page(&t->spt, va);
	if(page != NULL){
		return vm_do_claim_page (page);
	}else{
		return false;	// XXX: spt hash에 va를 가지는 페이지가 없을 때 처리 되돌아가서?
	}
}

/* Claim the PAGE and set up the mmu. */
/* spt에 있는 page 받고, frame 할당시켜서 pml4에 연결 */
static bool
vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame ();

	/* Set links */
	frame->page = page;
	page->frame = frame;

	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	struct thread *t = thread_current();
	if(pml4_get_page(t->pml4, page->va) == NULL && pml4_set_page(t->pml4, page->va, frame->kva, page->writable)){
		// stack의 경우 swap_in 과정이 진행되지 않아도 됨
		if(page->is_stack == true)
			return true;

		// 페이지 구조체 안의 page_operations 구조체를 통해 swap_int 함수 테이블에 값 넣기
		return swap_in (page, frame->kva);
	}
	else{
		return false;	///XXX: pml4에 세팅 실패시 마찬가지로 되돌아가서 처리하는지
	}

}

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt UNUSED) {
	hash_init(&spt->sup_hash, page_hash, page_less, NULL);
}

/* Copy supplemental page table from src to dst */
/* src의 spt를 dst의 spt 로 복사 */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED, struct supplemental_page_table *src UNUSED) {

	struct thread *child_t = thread_current();

	struct hash_iterator i;
	hash_first (&i, &src->sup_hash);
	while (hash_next (&i)) {
		struct page *parent = hash_entry (hash_cur (&i), struct page, hash_e);
		void *upage = parent->va;

		switch(VM_TYPE(parent->operations->type)){
			case VM_UNINIT:	// anon으로 세팅된 뒤 아직 lazy_load되기 전 상태 (aux 필요)
				if(parent->uninit.type == VM_ANON){
					struct lazy_load_aux *p_aux = parent->uninit.aux;
					struct lazy_load_aux *c_aux = (struct lazy_load_aux *)calloc(1, sizeof(struct lazy_load_aux));
					c_aux->file = file_duplicate(p_aux->file);
					c_aux->ofs = p_aux->ofs;
					c_aux->page_read_bytes = p_aux->page_read_bytes;
					c_aux->page_zero_bytes = p_aux->page_zero_bytes;

					vm_alloc_page_with_initializer(VM_ANON, upage, parent->writable, parent->uninit.init, c_aux);
					
					break;
				}
			default:	// lazy_load 된 뒤 상태(frame과의 연결과 frame 세팅 필요)
					vm_alloc_page_with_initializer(VM_ANON, upage, parent->writable, NULL, NULL);
				if(vm_claim_page(upage)){
					struct page *child = spt_find_page(dst, upage);
					memcpy(child->frame->kva, parent->frame->kva, PGSIZE);
				}
				break;
		}
		// if(hash_insert(&dst->sup_hash, hash_cur (&i)) == NULL){
		// 	return false;
		}
	return true;
	}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
}

/* 3-1 implementation */

/* Returns a hash value for page p. */
uint64_t page_hash (const struct hash_elem *p_, void *aux UNUSED) {
  const struct page *p = hash_entry (p_, struct page, hash_e);
  return hash_bytes (&p->va, sizeof p->va);
}

/* Returns true if page a precedes page b. */
bool page_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED) {
  const struct page *a = hash_entry (a_, struct page, hash_e);
  const struct page *b = hash_entry (b_, struct page, hash_e);

  return a->va < b->va;
}