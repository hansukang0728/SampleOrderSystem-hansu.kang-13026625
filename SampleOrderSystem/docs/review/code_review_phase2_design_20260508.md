# Design Review — phase2_design.md

**리뷰 일시**: 2026-05-08  
**리뷰 대상**: `docs/phase/phase2_design.md`  
**리뷰어**: design-doc-reviewer agent  
**종합 평가**: ⚠️ Needs Minor Revisions

---

## 🔴 Critical Issues

1. **`asBool()` 지원 여부 불확실** — `json_lite.h` API 목록에 미명시  
   **→ 조치**: `json_lite.h` 확인 결과 `asBool()` 지원 확인 ✅ (line 39)

2. **검증 시나리오에 `saveFile`/`loadFile` 왕복 누락** — CLAUDE.md Phase 2 기준과 불일치  
   **→ 조치**: 완료 기준에 "파일 I/O 왕복은 Phase 3에서 수행" 명시 ✅

## 조치 결과
- [x] asBool() 지원 확인
- [x] 파일 I/O 왕복 검증 Phase 3 이월 명시

---

## 🟠 Major Issues

1. **함수명 불일치**: `generateSampleId` vs `formatSampleId`  
   **→ 조치**: `formatSampleId` / `formatOrderId` 로 통일, 주석에 명칭 정책 기재 ✅

2. **`stockStatus` 파라미터명 `stock`이 멤버 필드와 충돌**  
   **→ 조치**: 파라미터명 `stock` → `qty` 로 변경 ✅

3. **`Order::quantity` 기본값 0이 유효 범위(>=1) 위반**  
   **→ 조치**: 주석으로 "AppDB/Service가 유효값 보장" 명시 ✅

## 조치 결과
- [x] formatSampleId/formatOrderId 명칭 통일
- [x] stockStatus 파라미터명 qty로 변경
- [x] quantity/id 기본값 의미 주석 추가

---

## 🟡 Minor Issues

1. `localtime_s` Windows 전용 — 이미 Windows 전용 프로젝트이므로 문서 주석으로 충분
2. `OrderStatus` 이중 선언 — 구현 코드 블록에서 enum 재선언은 설명용임을 인지
3. `ProductionQueueItem::id` 기본값 0 의미 불명확 → 주석 추가 ✅
4. `stringToStatus` 폴백 정책 미명시 → 주석으로 의도적 설계 명시 ✅
5. 검증 6.3 수치 근거 누락 → yield_rate=0.88 기반 계산식 추가 ✅

## 조치 결과
- [x] ProductionQueueItem::id 주석 추가
- [x] stringToStatus 폴백 정책 주석 추가
- [x] 6.3 수치 근거 (yield_rate=0.88, actual_qty=57) 추가

---

## ✅ Strengths

- 도메인 모델 필드가 CLAUDE.md 섹션 2와 완전 일치
- OrderStatus 상태 전이 다이어그램 CLAUDE.md와 동일
- stockStatus 임계값(0/1~10/11+) 정확히 일치
- 생산 계산 공식이 ProductionQueueItem 필드 설명에 명시
- 의존성 그래프 명확
- 유틸리티 함수 모두 inline 자유 함수 (헤더 온리 방침 부합)
