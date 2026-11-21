<?php

namespace App\Models;

use Illuminate\Database\Eloquent\Factories\HasFactory;
use Illuminate\Database\Eloquent\Model;


class Perangkat extends Model
{
    
    use HasFactory;
    protected $table = 'perangkats';

    protected $primaryKey = 'id_perangkat';

    protected $fillable = [
        'nama_brankas',
        'status_pintu', 
        'status_alarm'
    ];

    public function logs(){
        return $this->hasMany(LogAktivitas::class, 'id_perangkat', 'id_perangkat');
    }

}
