/*
 * answers.h - what the shipped routes answered while the suite ran
 *
 * Copyright (C) 2026 NI-Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef COREAPI_TEST_ANSWERS_H
#define COREAPI_TEST_ANSWERS_H

#include <map>
#include <string>

// As complete as the cases: a refusal no case provokes cannot be declared.
void watchAnswers();

bool answersAgree(const char *actual_path);

// fills names the segments the document gives no example for; from and to rewrite the body.
int sendBodyExample(const char *method, const char *path,
                    const std::map<std::string, std::string> &fills,
                    const std::string &from = std::string(),
                    const std::string &to = std::string());

#endif
