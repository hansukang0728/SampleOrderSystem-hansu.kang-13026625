// MVC.cpp
// MVC 아키텍처 코어 프로젝트
// - Model/  : json_lite.h, models.h (Sample·Order·ProductionQueueItem), app_db.h
// - View/   : ConsoleUI.h (공통 UI 유틸), *View.h (Phase 4-2 이후 구현)
// - Service/: *Service.h (Phase 4-2 이후 구현)
//
// 실행 진입점은 SampleOrderSystem 프로젝트(SampleOrderSystem.cpp)에 있습니다.
// 이 파일은 MVC 프로젝트 단독 빌드 확인용 플레이스홀더입니다.

#include <iostream>
#include "Model/models.h"
#include "Model/app_db.h"
#include "View/ConsoleUI.h"

int main() {
    std::cout << "MVC core project - build OK\n";
    std::cout << "Entry point: SampleOrderSystem project\n";
    return 0;
}
