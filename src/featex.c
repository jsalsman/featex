
// featex.c - PocketSphinx phonetic feature extraction for intelligibility prediction and remediation
// by James Salsman, July-August 2017
// released under the MIT open source license

#define INFILENAME "featex.raw"
#define FRATE 65
#define MODELDIR "/usr/local/share/pocketsphinx/model/en-us/en-us"
#define DICTNAME "combo.dict"

#include <pocketsphinx.h>
#include "ps_alignment.h"

#include "state_align_search.h"
#include "pocketsphinx_internal.h"
#include "ps_search.h"

#include <ctype.h>
#include <math.h>

int main(int argc, char *argv[])
{
    ps_decoder_t *ps;
    dict_t *dict;
    dict2pid_t *d2p;
    acmod_t *acmod;
    bin_mdef_t *mdef;
    ps_alignment_t *al;
    ps_alignment_iter_t *itor, *itor2;
    ps_search_t *search;
    cmd_ln_t *config;
    FILE *rawfh;
    int16 buf[2048];
    char *fbuf, *fbip, *obuf;
    size_t nread;
    int16 const *bptr;
    int sz, nfr, wend, n, maxdur, i, j, k, play, found;
    ps_alignment_entry_t *ae;
    char grammar[1000], target[10], frates[10];
    char *p, *q, *r; // string manipulation pointers for constructing grammar
    ps_nbest_t *nb;
    int32 score;
    hash_table_t *hyptbl;
    double frated;

    struct
    {
        int start, dur, cipid, score;
    } * algn;

    if (argc < 2 || (*argv[1] == '-' && (*(argv[1] + 1) != 'p' || argc < 3)))
    {
        fprintf(stderr, "usage: %s [-p[u][w][p][t][d]] word....\n"
                        "-p: play [u]tterance, [w]ord(s), [p]honemes (default), "
                        "[t]riphones, and/or [d]iphones.\n",
                argv[0]);
        return 1;
    }

    play = 0; // by default play nothing
    if (*argv[1] == '-' && *(argv[1] + 1) == 'p')
    {
        play = 4; // just '-p' means to only play phonemes
        if (*(argv[1] + 2))
        {
            play = 0;
            p = argv[1] + 1;
            while (*++p)
            {
                if (*p == 'u')
                    play |= 1; // utterance
                else if (*p == 'w')
                    play |= 2; // word(s)
                else if (*p == 'p')
                    play |= 4; // phonemes
                else if (*p == 't')
                    play |= 8; // triphones
                else if (*p == 'd')
                    play |= 16; // diphones
                else
                {
                    fprintf(stderr, "%s: unrecogized -p option selection;\n"
                                    "-p: play [u]tterance, [w]ord(s), [p]honemes (default),"
                                    " [t]riphones, and/or [d]iphones.\n",
                            argv[0]);
                    return 1;
                }
            }
        }
        i = 2;
    }
    else
    {
        i = 1;
    }

#define FPS (16000 / FRATE * 2)

    sprintf(frates, "%d", FRATE);
    frated = (double)FRATE;
    config = cmd_ln_init(NULL, ps_args(), FALSE,
                         "-hmm", MODELDIR,
                         "-dict", DICTNAME,
                         "-samprate", "16000",
                         "-topn", "64", // TODO parameterize for proper optimization
                         "-beam", "1e-57",
                         "-wbeam", "1e-56",
                         "-maxhmmpf", "-1",
                         "-frate", frates,
                         "-fsgusefiller", "no",
                         NULL);
    if (!(ps = ps_init(config)))
    {
        fprintf(stderr, "%s: ps_init() failed.\n", argv[0]);
        return 2;
    }
    dict = ps->dict;
    d2p = ps->d2p;
    acmod = ps->acmod;
    mdef = acmod->mdef;

    al = ps_alignment_init(d2p);
    ps_alignment_add_word(al, dict_wordid(dict, "<s>"), 0);
    while (i < argc)
    {
        n = dict_wordid(dict, argv[i]);
        if (n < 0)
        {
            fprintf(stderr, "%s: unrecogized word: %s\n", argv[0], argv[i]);
            return 3;
        }
        ps_alignment_add_word(al, n, 0);
        i++;
    }
    ps_alignment_add_word(al, dict_wordid(dict, "</s>"), 0);
    ps_alignment_populate(al);

    search = state_align_search_init("state_align", config, acmod, al);

    rawfh = fopen(INFILENAME, "rb");
    if (!rawfh)
    {
        fprintf(stderr, "%s: can't open audio input file: %s\n",
                argv[0], INFILENAME);
        return 4;
    }
    fseek(rawfh, 0L, SEEK_END);
    sz = ftell(rawfh);
    fbuf = fbip = malloc(sz);
    rewind(rawfh);
    while (!feof(rawfh))
    {
        nread = fread(buf, sizeof(*buf), 2048, rawfh);
        memcpy(fbip, buf, nread * sizeof(*buf));
        fbip += nread * sizeof(*buf);
    }

    acmod_start_utt(acmod);
    ps_search_start(search);

    bptr = (const int16 *)fbuf;
    nread = (fbip - fbuf) / sizeof(*buf);
    while ((nfr = acmod_process_raw(acmod, &bptr, &nread, TRUE)) > 0)
    {
        while (acmod->n_feat_frame > 0)
        {
            ps_search_step(search, acmod->output_frame);
            acmod_advance(acmod);
        }
        fprintf(stderr, "%s: processed %d frames\n", argv[0], nfr);
    }

    acmod_end_utt(acmod);
    ps_search_finish(search);

    fprintf(stderr, "%s: aligned %d words, %d phones, and %d states\n",
            argv[0], ps_alignment_n_words(al), ps_alignment_n_phones(al),
            ps_alignment_n_states(al));

    if (play & 1)
    { // play utterance
        rawfh = fopen("/tmp/outphone.raw", "wb");
        fwrite(fbuf, sz, 1, rawfh);
        fclose(rawfh);
        system("play -q -r16k -ts16 -c1 /tmp/outphone.raw");
        remove("/tmp/outphone.raw");
    }

    algn = malloc(sizeof(*algn) * ps_alignment_n_phones(al));
    obuf = malloc(8000);
    memset(obuf, 0, 8000);
    n = 0;

    maxdur = 0;

    itor = ps_alignment_words(al);
    while (itor)
    {
        ae = ps_alignment_iter_get(itor);
        fprintf(stderr, "%s: word '%s': %.2fs for %.2fs, score %d\n", argv[0],
                dict->word[ae->id.wid].word, ae->start / frated,
                ae->duration / frated, ae->score);
        if (play & 2)
        { // play words
            rawfh = fopen("/tmp/outphone.raw", "wb");
            fwrite(obuf, 8000, 1, rawfh);
            fwrite(fbuf + ae->start * FPS, ae->duration * FPS, 1, rawfh);
            fwrite(obuf, 8000, 1, rawfh);
            fclose(rawfh);
            system("play -q -r16k -ts16 -c1 /tmp/outphone.raw");
            remove("/tmp/outphone.raw");
        }
        itor2 = ps_alignment_iter_down(itor);
        wend = ae->duration + ae->start;
        while (itor2)
        {
            ae = ps_alignment_iter_get(itor2);
            if (ae->start >= wend)
                break;
            fprintf(stderr, "%s: sub-phone '%s': %.2fs for %.2fs, score %d\n",
                    argv[0], mdef->ciname[ae->id.pid.cipid], ae->start / frated,
                    ae->duration / frated, ae->score);
            algn[n].start = ae->start;
            algn[n].dur = ae->duration;
            algn[n].score = ae->score;
            algn[n++].cipid = ae->id.pid.cipid;
            if (ae->duration > maxdur)
                maxdur = ae->duration;
            itor2 = ps_alignment_iter_next(itor2);
        }
        itor = ps_alignment_iter_next(itor);
    }

    ps_search_free(search);
    ps_alignment_free(al);
    free(obuf);

    obuf = malloc(16000 + FPS * maxdur * 3);
    memset(obuf, 0, 8000);
    for (i = 0; i < n; i++)
    {
        memcpy(obuf + 8000, fbuf + algn[i].start * FPS, algn[i].dur * FPS);
        memset(obuf + 8000 + algn[i].dur * FPS, 0, 8000);

        fprintf(stderr, "%s: phoneme %d: %s %.2fs for %.2fs, score %d\n",
                argv[0], i + 1, mdef->ciname[algn[i].cipid],
                algn[i].start / frated, algn[i].dur / frated, algn[i].score);
        if (play & 4)
        { // play phonemes (default if '-p' specified)
            rawfh = fopen("/tmp/outphone.raw", "wb");
            fwrite(obuf, 16000 + algn[i].dur * FPS, 1, rawfh);
            fclose(rawfh);
            system("play -q -r16k -ts16 -c1 /tmp/outphone.raw");
            remove("/tmp/outphone.raw");
        }
    }

    hyptbl = hash_table_new(175, HASH_CASE_YES); // for hypothesis deduplication

    for (i = 1; i < n; i++)
    {

        if (i == n - 1)
            goto lastdiphone;

        if (i > 1)
            printf(" ");

        printf("%.2f %.3f", algn[i].dur / frated, 1 / log(2 - algn[i].score));

        memcpy(obuf + 8000, fbuf + algn[i - 1].start * FPS,
               (algn[i - 1].dur + algn[i].dur + algn[i + 1].dur) * FPS);
        memset(obuf + 8000 + (algn[i - 1].dur + algn[i].dur + algn[i + 1].dur) * FPS,
               0, 8000);

        fprintf(stderr, "%s: triphone %d: %s-%s-%s\n", argv[0], i,
                mdef->ciname[algn[i - 1].cipid],
                mdef->ciname[algn[i].cipid],
                mdef->ciname[algn[i + 1].cipid]);
        if (play & 8)
        { // play triphones
            rawfh = fopen("/tmp/outphone.raw", "wb");
            fwrite(obuf, 16000 + (algn[i - 1].dur + algn[i].dur + algn[i + 1].dur) * FPS, 1, rawfh);
            fclose(rawfh);
            system("play -q -r16k -ts16 -c1 /tmp/outphone.raw");
            remove("/tmp/outphone.raw");
        }

        grammar[0] = '\0';
        strcat(grammar, "#JSGF V1.0;\ngrammar subalts;\npublic <alts> = sil1 ");
        if (algn[i - 1].cipid != mdef->sil)
        {
            p = mdef->ciname[algn[i - 1].cipid];
            q = grammar;
            while (*++q)
                ;
            while (*p)
                *q++ = tolower(*p++);
            *q++ = '2';
            *q = '\0';
        }
        strcat(grammar, " [ aa3 | ae3 | ah3 | ao3 | aw3 | ay3 | b3 | ch3 | d3"
                        " | dh3 | eh3 | er3 | ey3 | f3 | g3 | hh3 | ih3 | iy3 | jh3"
                        " | k3 | l3 | m3 | n3 | ng3 | ow3 | oy3 | p3 | r3 | s3 | sh3"
                        " | sil3 | t3 | th3 | uh3 | uw3 | v3 | w3 | y3 | z3 | zh3 ] ");
        if (algn[i + 1].cipid != mdef->sil)
        {
            p = mdef->ciname[algn[i + 1].cipid];
            q = grammar;
            while (*++q)
                ;
            while (*p)
                *q++ = tolower(*p++);
            *q++ = '4';
            *q = '\0';
        }
        strcat(grammar, " sil5 ;\n");

        fprintf(stderr, "%s: %s", argv[0], grammar);

        ps_set_jsgf_string(ps, "subalts", grammar);
        ps_set_search(ps, "subalts");
        ps_start_utt(ps);
        ps_process_raw(ps, (const int16 *)obuf, 8000 + // samples not bytes
                                                    (algn[i - 1].dur + algn[i].dur + algn[i + 1].dur) * 160,
                       FALSE, TRUE);
        ps_end_utt(ps);

        nb = ps_nbest(ps);
        j = found = 0;
        target[0] = ' ';
        target[1] = '\0';
        strcat(target, mdef->ciname[algn[i].cipid]);
        strcat(target, "3");
        p = target;
        while (*++p)
        {
            *p = tolower(*p);
        }
        while (nb)
        {
            p = (char *)ps_nbest_hyp(nb, &score);
            if (p)
            { // some hypotheses are literally NULL
                q = p;
                while (*++q)
                    ;
                if (*(q - 1) == '5')
                { // ignore hypotheses w/o whole match

                    // ignore repeated hypotheses
                    if (hash_table_lookup(hyptbl, p, NULL) == -1)
                    {
                        j++;
                        fprintf(stderr, "%s: triphone hypothesis %d: %s, %d\n",
                                argv[0], j, p, score);
                        hash_table_enter_int32(hyptbl, p, score);

                        if (strstr(p, target))
                        {
                            found++;
                            ps_nbest_free(nb);
                            break;
                        }
                    }
                }
            }
            nb = ps_nbest_next(nb);
        }
        if (!found)
            j = 42; // zero for bad recognition results or no match
        fprintf(stderr, "%s: SUBSTITUTION: %.3f\n", argv[0], (42.0 - j) / 42.0);
        printf(" %.3f", (42.0 - j) / 42.0);

        hash_table_empty(hyptbl);

    lastdiphone: // goto target for the final set of two phonemes

        memcpy(obuf + 8000, fbuf + algn[i - 1].start * FPS,
               (algn[i - 1].dur + algn[i].dur) * FPS);
        memset(obuf + 8000 + (algn[i - 1].dur + algn[i].dur) * FPS, 0, 8000);

        fprintf(stderr, "%s: diphone %d: %s-%s\n", argv[0], i,
                mdef->ciname[algn[i - 1].cipid],
                mdef->ciname[algn[i].cipid]);
        if (play & 16)
        { // play diphones
            rawfh = fopen("/tmp/outphone.raw", "wb");
            fwrite(obuf, 16000 + (algn[i - 1].dur + algn[i].dur) * FPS, 1, rawfh);
            fclose(rawfh);
            system("play -q -r16k -ts16 -c1 /tmp/outphone.raw");
            remove("/tmp/outphone.raw");
        }

        grammar[0] = '\0';
        strcat(grammar,
               "#JSGF V1.0;\ngrammar insdels;\npublic <alts> = sil1 [ ");
        p = mdef->ciname[algn[i - 1].cipid];
        q = grammar;
        while (*++q)
            ;
        while (*p)
            *q++ = tolower(*p++);
        *q++ = '2';
        *q = '\0';
        r = q;
        strcat(grammar, " ] [  aa3| ae3 | ah3 | ao3 | aw3 | ay3 | b3  | ch3"
                        " | d3  | dh3 | eh3 | er3 | ey3 | f3  | g3  | hh3 | ih3 | iy3"
                        " | jh3 | k3  | l3  | m3  | n3  | ng3 | ow3 | oy3 | p3  | r3 "
                        " | s3  | sh3 | sil3 | t3  | th3 | uh3 | uw3 | v3  | w3  | y3 "
                        " | z3  | zh3 ] ");
        p = mdef->ciname[algn[i - 1].cipid]; // first in diphone
        while (*++q)
        { // blank out expected phoneme from possible insertions
            if (isalpha(*q))
            {
                if ((*q == tolower(*p)) && (((*(q + 1) == '3') && *(p + 1) == '\0') || *(q + 1) == tolower(*(p + 1))))
                {
                    *(q - 2) = ' '; // blank out preceding '|'
                    *q = ' ';
                    *(q + 1) = ' ';
                    *(q + 2) = ' ';
                    *(q + 3) = ' ';
                }
                else
                {
                    q += 3; // advance past the rest of the phoneme
                }
            }
        }
        p = mdef->ciname[algn[i].cipid]; // second in diphone
        q = r;
        while (*++q)
        { // blank out expected phoneme from possible insertions
            if (isalpha(*q))
            {
                if ((*q == tolower(*p)) && (((*(q + 1) == '3') && *(p + 1) == '\0') || *(q + 1) == tolower(*(p + 1))))
                {
                    *(q - 2) = ' '; // blank out preceding '|'
                    *q = ' ';
                    *(q + 1) = ' ';
                    *(q + 2) = ' ';
                    *(q + 3) = ' ';
                }
                else
                {
                    q += 3; // advance past the rest of the phoneme
                }
            }
        }
        if (algn[i].cipid != mdef->sil)
        {
            p = mdef->ciname[algn[i].cipid];
            q = grammar;
            while (*++q)
                ;
            while (*p)
                *q++ = tolower(*p++);
            *q++ = '4';
            *q = '\0';
        }
        strcat(grammar, " sil5 ;\n");

        fprintf(stderr, "%s: %s", argv[0], grammar);

        ps_set_jsgf_string(ps, "insdels", grammar);
        ps_set_search(ps, "insdels");
        ps_start_utt(ps);
        ps_process_raw(ps, (const int16 *)obuf, 8000 + // samples not bytes
                                                    (algn[i - 1].dur + algn[i].dur) * 160,
                       FALSE, TRUE);
        ps_end_utt(ps);

        nb = ps_nbest(ps);
        j = k = found = 0;
        while (nb)
        {
            p = (char *)ps_nbest_hyp(nb, &score);
            if (p)
            { // some hypotheses are literally NULL
                q = p;
                while (*++q)
                    ;
                if (*(q - 1) == '5')
                { // ignore hypotheses w/o whole match

                    // ignore repeated hypotheses
                    if (hash_table_lookup(hyptbl, p, NULL) == -1)
                    {
                        j++;
                        fprintf(stderr, "%s: diphone hypothesis %d: %s, %d\n",
                                argv[0], j, p, score);
                        hash_table_enter_int32(hyptbl, p, score);

                        if (!strstr(p, "2 "))
                            k++;
                        if (strstr(p, "3 "))
                            k++;

                        if (strstr(p, "2 ") && !strstr(p, "3 "))
                        {
                            found++;
                            ps_nbest_free(nb);
                            break;
                        }
                    }
                }
            }
            nb = ps_nbest_next(nb);
        }
        if (j == 0)
            k = 160; // zero for bad recognition results
        else if (!found)
        {
            k += 80; // add half the range if the preferred hypothesis missed
            if (k > 160)
                k = 160; // clamp
        }
        fprintf(stderr, "%s: INS/DEL: %.3f\n", argv[0], (160.0 - k) / 160);
        printf(" %.3f", (160.0 - k) / 160.0);

        hash_table_empty(hyptbl);
    }

    printf("\n");

    free(obuf);
    free(algn);
    free(fbuf);
    ps_free(ps);
    cmd_ln_free_r(config);

    return 0;
}
