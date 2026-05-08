# Code Review — models.h + tests/models_test.cpp (Phase 2)

**리뷰 일시**: 2026-05-08  
**리뷰 대상**: `models.h`, `tests/models_test.cpp`  
**리뷰어**: clean-code-reviewer agent

---

## 종합 평가
Phase 2 구현은 설계 문서와 높은 정합성을 보이며 21개 테스트가 모두 통과. 헤더 전용 설계, MSVC 안전 함수(`localtime_s`, `sscanf_s`) 사용, `started_at` 누락 하위 호환 처리는 의도가 명확한 좋은 선택. 전역 네임스페이스 오염 및 parseTime 실패값 처리가 후속 Phase의 잠재적 버그로 이어질 수 있어 보완 필요.

---

## Critical 🔴
없음

---

## Major 🟡

1. **DRY 위반**: `nowStr()` / `todayStr()` 중복 로직 → `localNow()` 헬퍼로 추출
2. **`parseTime()` 실패 시 epoch(0) 반환** → `isTimeElapsed()` 오동작 가능 → `-1` 반환 + `elapsedMinutes` 방어 추가
3. **전역 네임스페이스 오염**: 유틸리티 6개 함수가 전역에 노출 → `namespace sos` 도입

## 조치 결과
- [x] `localNow()` 내부 헬퍼 추출, `nowStr` / `todayStr` DRY 제거
- [x] `parseTime` 실패값 -1, `elapsedMinutes` 방어 추가
- [x] `namespace sos` 도입 + using 선언으로 기존 사용성 유지
- [x] `progressPct` 하한 0 클램핑 추가 (`std::max(pct, 0.0)`)

---

## Minor 🟢

1. `stockStatus` 음수 입력 미처리 → 실제 경로에서 발생 불가, 허용
2. `#include <algorithm>` 의도 미명시
3. `Order::statusToString` switch default 추가 → 적용 완료
4. 테스트 `ToFromJsonRoundTrip`에서 sample_id / enqueued_at 검증 누락

## 조치 결과
- [x] `statusToString` switch에 `default: return "UNKNOWN"` 추가
- [x] 테스트 필드 정렬 수정

---

## 리팩토링 제안 — 반영 완료
- `localNow()` 추출로 DRY 해소
- `parseTime` → `elapsedMinutes` 실패 전파 차단
- `progressPct` 상·하한 [0, 100] 범위 보장

---

## 긍정 사항 ✅
- CLAUDE.md / phase2_design.md 필드·함수와 100% 정합
- `started_at` `contains` 가드로 하위 호환 보장
- `stringToStatus` RESERVED 폴백 주석으로 의도 명확화
- 21개 테스트: 경계값·폴백·상태 왕복·키 누락 등 Phase 2 범위 충분히 커버
- MSVC 안전 함수 일관 사용
