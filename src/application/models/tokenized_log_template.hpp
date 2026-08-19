#pragma once

#include "application/models/log_template_analysis.hpp"
#include "domain/log_template.hpp"

namespace loggen::application {

struct TokenizedLogTemplate {
    domain::LogTemplate item;
    LogTemplateAnalysis analysis;
};

}
