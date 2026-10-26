import type React from 'react';
import {controlStyleFields,type ControlColors} from '../../../packages/visual/control-styles.js';
/** Per-field markers preserve existing defaults when a named style omits a color. */
export function controlColorProps(colors:ControlColors,reading=false){
 const fields=controlStyleFields.filter(key=>colors[key]!==undefined);
 const style=Object.fromEntries(fields.map(key=>['--control-'+key,colors[key]])) as React.CSSProperties;
 return {style,...(reading?{'data-reading-control-colors':fields.join(' ')}:{'data-control-colors':fields.join(' ')})};
}
