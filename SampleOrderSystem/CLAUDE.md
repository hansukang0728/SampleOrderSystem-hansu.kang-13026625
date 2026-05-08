# SampleOrderSystem — 요구사항 및 설계 기준

## 1. 시스템 개요

시료의 **등록 → 주문 → 생산 → 출고**까지 전체 라이프사이클을 관리하는 단일 콘솔 애플리케이션.

> 본 프로젝트는 솔루션 내 4개 POC 프로젝트(DataPersistence, DataMonitor, DummyDataGenerator, MVC)를 통합하여 구현한다.  
> 각 POC의 역할과 기여 내용 → **[docs/solution-projects.md](docs/solution-projects.md)**

---

## 2. 도메인 모델 및 데이터 구조

### 2.1 Sample (시료)

| 필드 | 타입 | 형식 / 범위 | 설명 |
|---|---|---|---|
| id | string | `S-001`, `S-002`, ... | 고유 식별자 (자동 생성) |
| name | string | — | 시료명 |
| avg_production_time | double | 분/개 (min/ea) | 개당 생산에 소요되는 평균 시간 |
| yield_rate | double | 0.0 ~ 1.0 | 정상 시료 / 총 생산 시료 비율 |
| stock | int | 0 이상 (ea) | 현재 등록된 재고 수량 |

### 2.2 Order (주문)

| 필드 | 타입 | 형식 / 범위 | 설명 |
|---|---|---|---|
| id | string | `ORD-YYYYMMDD-XXXX` | 주문번호 (자동 생성) |
| sample_id | string | `S-NNN` | 대상 시료 ID |
| quantity | int | 1 이상 (ea) | 주문 수량 |
| customer_name | string | — | 주문자(고객)명 |
| status | OrderStatus | 아래 상태표 참고 | 현재 주문 상태 |
| created_at | string | `YYYY-MM-DD HH:MM:SS` | 주문 생성 시각 |

#### 주문 상태 흐름 (OrderStatus)

```
RESERVED → (승인, 재고 충분) → CONFIRMED
         → (승인, 재고 부족) → PRODUCING → (생산 완료) → CONFIRMED
         → (거절)           → REJECTED

CONFIRMED → (출고) → RELEASE
```

| 상태 | 설명 |
|---|---|
| RESERVED | 주문 접수 직후 초기 상태 |
| REJECTED | 주문 거절 상태 (모니터링 목록에서 제외) |
| PRODUCING | 승인 완료 후 재고 부족으로 생산 중 |
| CONFIRMED | 재고 확보 완료, 출고 대기 중 |
| RELEASE | 출고 완료, 최종 상태 |

### 2.3 ProductionQueueItem (생산 큐 항목)

| 필드 | 타입 | 설명 |
|---|---|---|
| id | int | 고유 식별자 |
| order_id | string | 연결된 주문 ID (`ORD-...`) |
| sample_id | string | 생산할 시료 ID (`S-NNN`) |
| shortage | int | 부족 수량 (ea) |
| actual_qty | int | 실 생산량 — `ceil(부족분 / (수율 × 0.9))` |
| total_time | double | 총 생산시간 (분) — `평균생산시간 × 실생산량` |
| completed | bool | 생산 완료 여부 |
| enqueued_at | string | 큐 등록 시각 (`YYYY-MM-DD HH:MM:SS`) |
| **started_at** | string | **실제 생산 시작 시각** — 비어있으면 WAITING 상태 |

#### 생산 상태 (3단계)

| 상태 | 조건 | 설명 |
|---|---|---|
| WAITING | `started_at == ""` && `completed == false` | 큐 대기 중 |
| IN_PROGRESS | `started_at != ""` && `completed == false` | 생산 중 (경과 시간 추적) |
| DONE | `completed == true` | 생산 완료, 재고·주문 상태 자동 갱신됨 |

조회·모니터링 시마다 IN_PROGRESS 항목의 경과 시간(`elapsedMinutes`)을 확인하여  
`경과 시간 >= total_time` 이면 자동 완료 처리: `stock += actual_qty`, 주문 → CONFIRMED

---

## 3. 핵심 비즈니스 로직

### 주문 승인 프로세스
```
승인 요청 (RESERVED 주문)
    │
    ├─ 재고 >= 주문수량  →  stock -= quantity
    │                       상태: CONFIRMED
    │
    └─ 재고 < 주문수량   →  부족분 = quantity - stock
                            생산 큐에 등록
                            상태: PRODUCING
```

### 생산 계산 공식
```
실 생산량  = ceil(부족분 / (수율 × 0.9))
총 생산시간 = 평균생산시간 × 실 생산량
```

### 생산 라인 처리 (FIFO)
- 단일 라인, 선입선출 방식
- `processNext()` 호출 시 큐 앞단 항목 처리
- 처리 완료 시: `stock += actual_qty`, 주문 상태 `PRODUCING → CONFIRMED`

### 재고 상태 임계값
| 상태 | 조건 |
|---|---|
| 고갈 | stock == 0 |
| 부족 | 1 <= stock <= 10 |
| 여유 | stock >= 11 |

---

## 4. 기능 요구사항

> 상세 기능 목록, 우선순위, 완료 기준은 **[PRD.md](PRD.md)** 를 기준 문서로 한다.  
> 기능별 상세 설계는 **`docs/feature/`** 디렉터리의 개별 문서를 참조한다.

### 기능 영역 요약

| 영역 | 주요 기능 | 상세 문서 |
|---|---|---|
| 메인 메뉴 | 전체 진입점 구성 및 UI 규칙 | [main.md](docs/feature/main.md) |
| 시료 관리 | 등록·전체조회·이름검색·재고상태 표시 | [sampleManage.md](docs/feature/sampleManage.md) |
| 시료 주문 | 주문 생성 (RESERVED) | [sampleOrder.md](docs/feature/sampleOrder.md) |
| 주문 승인/거절 | 재고 분기 승인·거절 처리 | [OrderManager.md](docs/feature/OrderManager.md) |
| 모니터링 | 재고·주문·생산 큐 대시보드 | [Monitoring.md](docs/feature/Monitoring.md) |
| 생산 라인 | FIFO 큐 조회·처리·재고 갱신 | [ProductionLine.md](docs/feature/ProductionLine.md) |
| 출고 처리 | CONFIRMED → RELEASE 처리 | [Release.md](docs/feature/Release.md) |

---

## 5. Agent 운용 규칙 (Mandatory)

> 아래 규칙은 선택이 아닌 **필수**다. 해당 조건이 충족되면 반드시 Agent를 호출한다.

### 규칙 1 — 문서 작성·수정 완료 시 → design-doc-reviewer 호출

**트리거 조건**
- CLAUDE.md, PRD.md, plan.md 작성 또는 수정 완료 시
- `docs/phase/phaseN_design.md` 작성 완료 시
- `docs/feature/*.md` 작성 또는 수정 완료 시

**수행 절차**
1. 문서 작성·수정 완료 직후 `design-doc-reviewer` agent 호출
2. agent가 반환한 리뷰 결과(Critical / Major / Minor / Suggestion)를 확인
3. **Critical · Major 항목은 반드시 문서에 반영** 후 재검토
4. Minor · Suggestion은 판단하여 선택 반영

**목적**: 구현 전 설계 오류·누락·모순을 문서 단계에서 조기 발견

---

### 규칙 2 — 코드 작성·수정 완료 시 → clean-code-reviewer 호출 + 리뷰 문서화 + 정합성 확인

**트리거 조건**
- 새로운 헤더(`.h`) 또는 소스(`.cpp`) 파일 작성 완료 시
- 기존 파일에 함수·클래스 추가 또는 수정 완료 시

**수행 절차**

**Step 1 — clean-code-reviewer agent 호출**
- 작성·수정된 코드를 대상으로 `clean-code-reviewer` agent 호출
- agent는 Clean Code 원칙·SOLID·C++ 관용구 기준으로 리뷰 수행

**Step 2 — 리뷰 결과 문서화**
- 리뷰 결과를 아래 경로에 문서로 저장:
  ```
  docs/review/code_review_<대상파일명>_<YYYYMMDD>.md
  ```
  예) `docs/review/code_review_models_h_20260508.md`

- 문서 포맷:
  ```markdown
  # Code Review — <대상 파일>
  **리뷰 일시**: YYYY-MM-DD
  **리뷰 대상**: <파일 경로>
  **리뷰어**: clean-code-reviewer agent

  ## 종합 평가
  <2~3문장 전체 평가>

  ## Critical 🔴
  <필수 수정 항목>

  ## Major 🟡
  <수정 권장 항목>

  ## Minor 🟢
  <선택 개선 항목>

  ## 리팩토링 제안
  <Before / After 코드 예시 (상위 1~3개)>

  ## 긍정 사항 ✅
  <잘 작성된 부분>

  ## 조치 결과
  - [ ] <Critical 항목 조치 여부>
  ```

**Step 3 — 리뷰 결과 보고 및 반영**
- Critical 항목: **즉시 코드 수정 후** 조치 결과를 문서에 `[x]` 로 표시
- Major 항목: 현재 Phase 완료 전까지 반영
- Minor · 리팩토링 제안: 다음 Phase 또는 판단에 따라 반영

**Step 4 — 문서 정합성 체크리스트 확인** (코드 ↔ 설계 문서 일치 여부)

| 확인 항목 | 참조 문서 |
|---|---|
| 클래스·메서드 이름이 설계와 일치하는가 | `plan.md`, `docs/phase/phaseN_design.md` |
| 도메인 모델 필드명·타입이 일치하는가 | `CLAUDE.md` 섹션 2 |
| ID 형식(`S-NNN`, `ORD-YYYYMMDD-XXXX`)이 코드에 반영됐는가 | `CLAUDE.md` 섹션 2 |
| OrderStatus 상태 전이 로직이 명세와 일치하는가 | `CLAUDE.md` 섹션 3, `docs/feature/OrderManager.md` |
| 생산 계산 공식이 정확히 구현됐는가 | `CLAUDE.md` 섹션 3, `docs/feature/ProductionLine.md` |
| 재고 상태 임계값(0/1~10/11+)이 코드에 반영됐는가 | `CLAUDE.md` 섹션 3 |
| PRD 기능 ID(S-01, O-02 등)에 대응하는 기능이 구현됐는가 | `PRD.md` |

- 정합성 불일치 발견 시 → **코드 또는 문서 중 하나를 수정하여 일치**시킨다
- 코드가 의도적으로 설계를 변경한 경우 → 문서를 먼저 업데이트한 뒤 코드 반영

---

### 규칙 3 — Phase 완료 시 종합 점검

빌드 성공만으로 Phase 완료를 선언하지 않는다. 아래 3단계를 모두 통과해야 완료다.

**Step 1 — 빌드 검증**
- Debug x64 기준 경고 0, 오류 0

**Step 2 — 실행 검증** (Phase별 기준은 `plan.md` 검증 레벨 컬럼 참조)

| Phase | 실행 검증 내용 |
|---|---|
| 1 | 초기화 메시지 정상 출력, 한글 깨짐 없음 |
| 2 | toJson → saveFile → loadFile → fromJson 왕복 후 값 일치 |
| 3 | 실행 후 data.json 생성 확인, 재실행 후 데이터 유지 확인 |
| 4 | 재고 충분·부족 승인 분기, 생산 계산 공식(`ceil(부족분/(수율×0.9))`) 수치 검증 |
| 5 | 전체 메뉴 진입·입력·출력 흐름, PRD 기능 ID별 동작 확인 |
| 6 | PRD DoD 체크리스트 전 항목 수동 확인 |
| 7 | 프로그램 재실행 후 모든 데이터 유지, 엣지 케이스(재고 0, 중복 승인 등) |

**Step 3 — 문서 점검**
- `design-doc-reviewer` 로 해당 Phase 설계 문서 최종 검토
- `clean-code-reviewer` 로 해당 Phase 전체 코드 검토 → 리뷰 문서 생성
- `docs/review/` 내 미조치 Critical·Major 항목 전수 확인
- 모든 항목 완료 후 `plan.md` 해당 Phase 체크리스트를 `[x]` 로 표시

**리뷰 문서 관리**
- `docs/review/` 디렉터리에 파일별 리뷰 문서 누적 관리
- 파일명 규칙: `code_review_<파일명>_<YYYYMMDD>.md`
- 동일 파일 재리뷰 시 새 날짜로 신규 문서 생성 (이력 보존)

---

## 6. Agent 목록 및 역할

이 프로젝트에는 코드 품질과 설계 문서 품질을 자동으로 검토하는 두 개의 Agent가 등록되어 있습니다.
Agent 정의 파일 위치: `DummyDataGenerator/.claude/agents/`

---

### 🔴 design-doc-reviewer

| 항목 | 내용 |
|---|---|
| 파일 | `design-doc-reviewer.md` |
| 모델 | Sonnet |
| 메모리 | project 범위 |

**역할**  
설계 문서(아키텍처, 시스템 설계, API 설계, 기술 사양, 기능 설계서)가 작성되거나 수정된 후 구조적 피드백을 제공합니다. 구현 시작 전 문서 검토에 사용합니다.

**검토 항목**
- **완전성**: 누락된 섹션, 미정의 요구사항 식별
- **명확성**: 모호한 표현, 다중 해석 가능한 문장 지적
- **일관성**: 문서 내 모순(데이터 모델 충돌, 인터페이스 불일치 등)
- **아키텍처 리스크**: 성능 병목, 강결합, 확장성 문제 가능성
- **요구사항 추적성**: 설계가 요구사항을 빠짐없이 반영하는지 확인
- **엣지 케이스**: 오류 조건, 실패 시나리오 누락 여부
- **실현 가능성**: 제안된 설계의 현실적 구현 가능성 평가

**출력 형식**: Critical / Major / Minor / Suggestion 4단계 심각도로 분류된 리뷰 리포트

**트리거 시점**
- CLAUDE.md, PRD.md 작성·수정 완료 후
- 새로운 기능 설계서 작성 후 구현 전

---

### 🔵 clean-code-reviewer

| 항목 | 내용 |
|---|---|
| 파일 | `clean-code-reviewer.md` |
| 모델 | Sonnet |
| 메모리 | project 범위 |

**역할**  
최근 작성하거나 수정한 코드를 대상으로 Clean Code 원칙 준수 여부를 검토합니다. 함수·클래스·모듈 단위 작업 완료 후 또는 PR 제출 전에 사용합니다.

**검토 항목**
- **네이밍**: 의도를 드러내는 이름, 약어·노이즈 워드 제거
- **함수·메서드**: 단일 책임, 짧고 집중된 함수, 인수 최소화
- **클래스·구조**: SRP 준수, 응집도, 인터페이스 최소화
- **주석**: 자기 설명적 코드 우선, 불필요한 주석 제거
- **에러 처리**: 명시적·우아한 오류 처리, 비즈니스 로직과 분리
- **코드 스멜**: 긴 메서드, 중복, 데드 코드, 원시 타입 집착
- **SOLID 원칙**: 단일책임·개방폐쇄·리스코프·인터페이스분리·의존역전
- **C++ 특화**: `const`/RAII/스마트포인터/Rule of Zero·Three·Five, Modern C++ 관용구

**출력 형식**: Critical 🔴 / Major 🟡 / Minor 🟢 + 상위 1~3개 리팩토링 Before/After 예시

**트리거 시점**
- 새 함수·클래스·모듈 구현 완료 후
- 기존 코드 리팩토링 후
- PR 제출 전

---

## 6. 비기능 및 구현 요구사항

### 5.1 기술 스택
- **언어**: C++20 (`stdcpp20`) — vcxproj 기준
- **영속성**: JSON 파일 (`data.json`) — `json_lite.h` 사용, 외부 라이브러리 없음
- **인코딩**: `/utf-8` 컴파일러 플래그, `SetConsoleOutputCP(CP_UTF8)`
- **전처리**: `NOMINMAX` 정의
- **아키텍처**: POC(DataPersistence, MVC)의 구조를 통합하여 단일 프로젝트로 구현
- **UI**: 콘솔 메뉴 기반 인터랙티브 앱

### 5.2 데이터 영속성
- 파일(JSON) 기반 저장으로 **프로그램 재실행 후에도 데이터 유지**
- `data.json` 단일 파일에 Sample, Order, ProductionQueueItem 전체 저장
- 실행 시 자동 로드, 변경 시 즉시 저장 (write-through)

### 5.3 더미 데이터
- 테스트를 위한 기초 시료 및 주문 데이터 **자동 생성 기능 포함**
- 초기 실행 시 또는 별도 명령으로 더미 데이터 투입 가능
- 생성 항목: 시료 N개 (이름·평균생산시간·수율·재고 난수), 주문 M건 (임의 시료·수량·고객명)

### 5.4 Clean Code / Agentic Engineering
- **CLAUDE.md**: 도메인 모델, 비즈니스 로직, 기술 요구사항 기준 문서 (현재 파일)
- **PRD.md**: 제품 요구사항 문서 — 기능 범위, 우선순위, 완료 기준 관리
- **plan.md**: 7단계 구현 계획 — Phase별 작업 항목 및 설계 문서 링크
- **docs/phase/phaseN_design.md**: 각 Phase 상세 설계 문서
  - Phase 1: 프로젝트 기반 설정 → [phase1_design.md](docs/phase/phase1_design.md)
  - Phase 2~7: 구현 진행에 따라 순차 작성
- **docs/feature/**: 기능별 UI·흐름·검증 규칙 문서
- **docs/solution-projects.md**: POC 프로젝트별 기여 관계 정리
- 코드 변경 시 위 문서와 일관성 유지
- 함수·클래스 단일 책임 원칙 준수, 불필요한 추상화 금지
