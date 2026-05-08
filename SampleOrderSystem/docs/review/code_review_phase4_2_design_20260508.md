# Design Review — phase4_2_samplemanage_design.md

**리뷰 일시**: 2026-05-08  
**리뷰 대상**: `docs/phase/phase4_2_samplemanage_design.md`  
**리뷰어**: design-doc-reviewer agent  
**종합 평가**: ⚠️ Minor Revisions

---

## 🔴 Critical Issues

1. **C-01**: 섹션 3.1 `searchByName` 시그니처가 구버전(`Sample*`)으로 남아있음 — 섹션 3.3에서 `const Sample*`로 수정했으나 3.1에 미반영
2. **C-02**: PRD.md S-05가 여전히 필수로 남아있음 — 모니터링 이관 결정이 PRD에 미반영

## 조치 결과
- [x] 섹션 3.1 인터페이스를 `std::vector<const Sample*>` const로 수정
- [x] 섹션 3.3 const_cast 코드 블록 제거, const 버전만 남김
- [x] PRD.md S-05 → 모니터링 이관 명시

---

## 🟠 Major Issues

1. **M-01**: 유효성 검증 오류 메시지 출력 주체 불명확 (Service vs View)
2. **M-02**: gtest Fixture raw pointer double-free 위험 (TC-SS-07)

## 조치 결과
- [x] 섹션 3.2에 "오류 메시지 출력은 View 책임, Service는 bool만 반환" 명시
- [x] gtest Fixture unique_ptr로 교체, TC-SS-07 reset() 패턴 적용

---

## 🟡 Minor Issues

1. N-01: 섹션 5에 임시 객체 방식(A)과 권장 방식(B) 혼재
2. N-02: add() 후 searchByName() 결과 포인터 무효화 가능성 미명시
3. N-03: handleSearch() 출력 포맷이 handleListAll()과 달라 혼란

## 조치 결과
- [x] 섹션 5 방식 A 제거, 방식 B만 남김
- [x] TC-SS-06에 validateYieldRate(1.0) 경계값 테스트 추가

---

## ✅ Strengths
- MVC 레이어 분리 명확
- 출력 예시 구체적
- 7개 테스트가 CRUD + 영속성 전체 커버
- 의존성 다이어그램 간결·명확
