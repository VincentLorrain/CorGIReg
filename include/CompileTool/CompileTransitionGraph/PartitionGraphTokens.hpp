#pragma once

namespace CorGIReg {
    /**
     * @brief enum for all types of token use in the of the regex
     * 7-5 type
     * 4-0 id
    */
    enum class tGTokensTypes
    {
        STOP,
        NEXT,   /**< -> */
        PREC,   /**< <- */

        QOM,    /**< + */
        QZM,    /**< * */
        QZO,    /**< ?*/

        KEY,    /**< [A-Za-z_0-9]+ */ //a unique key in the graph 
        CKEY,   /**< [A-Za-z_0-9]+#[0-9]* */ // a key that is common to different query
        SCKEY,   /**< [A-Za-z_0-9]+$[0-9]* */

        SEP,    /**< ; */
        LPAREN, /**< \( */
        RPAREN, /**< \) */
        END, /**< $*/
    };

}

