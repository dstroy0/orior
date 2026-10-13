// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// broken.cu: the broken: ours held against the answer question by question, each place they part traced to the forms that wrote it
#include "identity_internal.h"

// The answer and ours, question by question. nvcc's chain for a question of the stick is the answer; ours is the
// disassembly of what the transpiler wrote for it, derived from the rulesets alone, and the rulesets are what the L*
// query gave. A question is one product, and the only reason to fold is to make the same product in fewer steps: two
// chains of as many steps are two arrangements of it and neither is broken, whatever order or pipe each takes. Where
// nvcc makes it in fewer steps there is a fold the query has not learned, and where ours takes fewer, ours folds
// further.
//
// The two chains are aligned link by link on their kinds, the longest run in common kept in order, and where they part
// each link of ours is moved (nvcc writes its kind elsewhere in the question) or ours alone (nvcc writes nothing of its
// kind there), each with the forms of the rulesets whose texts write it, and each link of nvcc's with nothing of ours
// is written with its address. The run's blob broken.txt holds a question a group of lines, its verdict and the steps
// of each, then each form whose links stand alone in a question nvcc makes in fewer steps, with its count of questions
// and of links, the most questions first
int identity_broken(const char *listing, const char *manifest, const std::string &folder,
                           const std::vector<std::string> &ours_paths, int count, char **files)
{
    std::vector<Question> questions;
    if (!questions_read(listing, manifest, &questions))
    {
        return 1;
    }
    std::vector<std::pair<std::string, std::vector<std::regex>>> forms;
    if (!forms_read(count, files, &forms))
    {
        return 1;
    }
    std::map<std::string, size_t> numbered;
    for (size_t at = 0u; at < questions.size(); at += 1u)
    {
        numbered[questions[at].number] = at;
    }
    std::vector<Question> ours;
    for (const std::string &path : ours_paths)
    {
        candidates_read(path, &ours);
    }
    const std::string path = folder + "/broken.txt";
    RunWritten file = {path, 0, std::string()};
    std::map<std::string, FormFolded> folded;
    unsigned int alike = 0u;
    unsigned int same = 0u;
    unsigned int theirs_fewer = 0u;
    unsigned int ours_fewer = 0u;
    unsigned int steps_to_learn = 0u;
    unsigned int unanswered = 0u;
    for (const Question &mine : ours)
    {
        const auto found = numbered.find(mine.number);
        if ((found == numbered.end()) || mine.chain.empty())
        {
            unanswered += 1u;
            continue;
        }
        const Question &answer = questions[found->second];
        const size_t rows = mine.chain.size();
        const size_t columns = answer.chain.size();
        // the longest run of kinds the two chains hold in common, in order
        std::vector<std::vector<unsigned int>> common(rows + 1u, std::vector<unsigned int>(columns + 1u, 0u));
        for (size_t row = rows; row-- > 0u;)
        {
            for (size_t column = columns; column-- > 0u;)
            {
                common[row][column] = (mine.chain[row].kind == answer.chain[column].kind)
                                          ? common[row + 1u][column + 1u] + 1u
                                          : std::max(common[row + 1u][column], common[row][column + 1u]);
            }
        }
        // the links each chain holds alone, and the stretches they part in, each running from one link in common to the
        // next
        std::vector<size_t> mine_only;
        std::vector<size_t> answer_only;
        std::vector<std::pair<std::vector<size_t>, std::vector<size_t>>> stretches(1u);
        for (size_t row = 0u, column = 0u; (row < rows) || (column < columns);)
        {
            if ((row < rows) && (column < columns) && (mine.chain[row].kind == answer.chain[column].kind))
            {
                if (!stretches.back().first.empty() || !stretches.back().second.empty())
                {
                    stretches.emplace_back();
                }
                row += 1u;
                column += 1u;
            }
            else if ((column < columns) && ((row == rows) || (common[row][column + 1u] >= common[row + 1u][column])))
            {
                answer_only.push_back(column);
                stretches.back().second.push_back(column);
                column += 1u;
            }
            else
            {
                mine_only.push_back(row);
                stretches.back().first.push_back(row);
                row += 1u;
            }
        }
        // a link of ours alone whose kind nvcc writes alone elsewhere in the question is moved, and the two stand for
        // each other; what is left of a stretch is its own, and a stretch where ours holds more of its own links than
        // nvcc's is where a fold takes steps away
        std::vector<int> moved(rows, 0);
        std::vector<int> paired(columns, 0);
        std::map<std::string, std::vector<size_t>> answer_kinds;
        for (const size_t column : answer_only)
        {
            answer_kinds[answer.chain[column].kind].push_back(column);
        }
        for (const size_t row : mine_only)
        {
            std::vector<size_t> &held = answer_kinds[mine.chain[row].kind];
            if (!held.empty())
            {
                moved[row] = 1;
                paired[held.front()] = 1;
                held.erase(held.begin());
            }
        }
        std::vector<int> folding(rows, 0);
        for (const auto &stretch : stretches)
        {
            const size_t mine_own = (size_t)std::count_if(stretch.first.begin(), stretch.first.end(),
                                                          [&](size_t row) { return !moved[row]; });
            const size_t answer_own = (size_t)std::count_if(stretch.second.begin(), stretch.second.end(),
                                                            [&](size_t column) { return !paired[column]; });
            for (const size_t row : stretch.first)
            {
                folding[row] = !moved[row] && (mine_own > answer_own);
            }
        }
        const char *verdict = "same";
        if (mine_only.empty() && answer_only.empty())
        {
            verdict = "alike";
            alike += 1u;
        }
        else if (columns < rows)
        {
            verdict = "fewer theirs";
            theirs_fewer += 1u;
            steps_to_learn += (unsigned int)(rows - columns);
        }
        else if (rows < columns)
        {
            verdict = "fewer ours";
            ours_fewer += 1u;
        }
        else
        {
            same += 1u;
        }
        run_print(&file, "question %s %s %u %u %u\n", mine.number.c_str(), verdict, (unsigned int)rows,
                  (unsigned int)columns, common[0][0]);
        for (const size_t row : mine_only)
        {
            const Link &link = mine.chain[row];
            const std::string names = forms_writing(forms, link.kind);
            run_print(&file, "%s %s %s form %s%s\n", moved[row] ? "moved" : "ours", link.position.c_str(),
                      link.kind.c_str(), names.c_str(), folding[row] ? " fold" : "");
            // only a link of a stretch where ours holds more of its own links than nvcc's, in a question nvcc makes in
            // fewer steps, is a step a fold takes away
            if (!folding[row] || (columns >= rows))
            {
                continue;
            }
            std::stringstream split(names);
            std::string name;
            while (split >> name)
            {
                FormFolded &form = folded[name];
                form.questions.insert(mine.number);
                form.alone += 1u;
            }
        }
        for (const size_t column : answer_only)
        {
            run_print(&file, "theirs %s:%s %s\n", answer.number.c_str(), answer.chain[column].position.c_str(),
                      answer.chain[column].kind.c_str());
        }
    }
    std::vector<std::pair<std::string, FormFolded>> ranked(folded.begin(), folded.end());
    std::sort(ranked.begin(), ranked.end(),
              [](const std::pair<std::string, FormFolded> &one, const std::pair<std::string, FormFolded> &other)
              {
                  return (one.second.questions.size() != other.second.questions.size())
                             ? (one.second.questions.size() > other.second.questions.size())
                             : (one.first < other.first);
              });
    for (const auto &form : ranked)
    {
        run_print(&file, "fold %s %u %u\n", form.first.c_str(), (unsigned int)form.second.questions.size(),
                  form.second.alone);
    }
    if (!run_closed(&file))
    {
        printf("  %s could not be written\n", path.c_str());
        return 1;
    }
    printf("  %s: %u of ours, %u alike link for link, %u in as many steps, %u nvcc makes in fewer (%u steps to "
           "learn), %u ours makes in fewer, %u with no answer\n",
           path.c_str(), (unsigned int)ours.size(), alike, same, theirs_fewer, steps_to_learn, ours_fewer, unanswered);
    for (size_t at = 0u; (at < ranked.size()) && (at < 8u); at += 1u)
    {
        printf("    %s: %u questions, %u links alone\n", ranked[at].first.c_str(),
               (unsigned int)ranked[at].second.questions.size(), ranked[at].second.alone);
    }
    return 0;
}
